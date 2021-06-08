/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xia-chu/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TCPSERVER_TCPSERVER_H
#define TCPSERVER_TCPSERVER_H

#include <assert.h>
#include <mutex>
#include <memory>
#include <exception>
#include <functional>
#include <unordered_map>

#include "Network/Server.h"
#include "Network/TcpSession.h"
#include "Poller/Timer.h"
#include "Thread/semaphore.h"
#include "Util/logger.h"
#include "Util/util.h"
#include "Util/uv_errno.h"

using namespace std;

namespace toolkit {

//TCP服务器，可配置的；配置通过TcpSession::attachServer方法传递给会话对象
//该对象是非线程安全的，务必在主线程中操作
class TcpServer : public Server {
public:
    typedef std::shared_ptr<TcpServer> Ptr;

    /**
     * 创建tcp服务器，listen fd的accept事件会加入到所有的poller线程中监听
     * 在调用TcpServer::start函数时，内部会创建多个子TcpServer对象，
     * 这些子TcpServer对象通过Socket对象克隆的方式在多个poller线程中监听同一个listen fd
     * 这样这个TCP服务器将会通过抢占式accept的方式把客户端均匀的分布到不同的poller线程
     * 通过该方式能实现客户端负载均衡以及提高连接接收速度
     */
    TcpServer(const EventPoller::Ptr &poller = nullptr) : Server(poller) {
        setOnCreateSocket(nullptr);
        _socket = createSocket();
        _socket->setOnAccept(bind(&TcpServer::onAcceptConnection_l, this, placeholders::_1));
        _socket->setOnBeforeAccept(bind(&TcpServer::onBeforeAcceptConnection_l, this, std::placeholders::_1));
    }

    virtual ~TcpServer() {
        if (!_cloned && _socket->rawFD() != -1) {
            InfoL << "close tcp server " << _socket->get_local_ip() << ":" << _socket->get_local_port();
        }
        _timer.reset();
        //先关闭socket监听，防止收到新的连接
        _socket.reset();
        _session_map.clear();
        _cloned_server.clear();
    }

    //开始监听服务器
    template <typename SessionType>
    void start(uint16_t port, const std::string &host = "0.0.0.0", uint32_t backlog = 1024) {
        start_l<SessionType>(port, host, backlog);
        EventPollerPool::Instance().for_each([&](const TaskExecutor::Ptr &executor) {
            EventPoller::Ptr poller = dynamic_pointer_cast<EventPoller>(executor);
            if (poller == _poller || !poller) {
                return;
            }
            auto &serverRef = _cloned_server[poller.get()];
            if (!serverRef) {
                serverRef = onCreatServer(poller);
            }
            if (serverRef) {
                serverRef->cloneFrom(*this);
            }
        });
    }

    /**
     * 获取服务器监听端口号，服务器可以选择监听随机端口
     */
    uint16_t getPort(){
        if (!_socket) {
            return 0;
        }
        return _socket->get_local_port();
    }

    void setOnCreateSocket(Socket::onCreateSocket cb) {
        if (cb) {
            _on_create_socket = std::move(cb);
        } else {
            _on_create_socket = [](const EventPoller::Ptr &poller) {
                return Socket::createSocket(poller, false);
            };
        }
        for (auto &pr : _cloned_server) {
            pr.second->setOnCreateSocket(cb);
        }
    }

protected:
    virtual TcpServer::Ptr onCreatServer(const EventPoller::Ptr &poller) {
        return std::make_shared<TcpServer>(poller);
    }

    virtual Socket::Ptr onBeforeAcceptConnection(const EventPoller::Ptr &poller) {
        assert(_poller->isCurrentThread());
        return createSocket();
    }

    virtual void cloneFrom(const TcpServer &that) {
        if (!that._socket) {
            throw std::invalid_argument("TcpServer::cloneFrom other with null socket!");
        }
        _on_create_socket = that._on_create_socket;
        _session_alloc = that._session_alloc;
        _socket->cloneFromListenSocket(*(that._socket));
        weak_ptr<TcpServer> weak_self = std::dynamic_pointer_cast<TcpServer>(shared_from_this());
        _timer = std::make_shared<Timer>(2.0f, [weak_self]() -> bool {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return false;
            }
            strong_self->onManagerSession();
            return true;
        }, _poller);
        this->mINI::operator=(that);
        _cloned = true;
    }

    // 接收到客户端连接请求
    virtual void onAcceptConnection(const Socket::Ptr &sock) {
        assert(_poller->isCurrentThread());
        weak_ptr<TcpServer> weak_self = std::dynamic_pointer_cast<TcpServer>(shared_from_this());
        //创建一个TcpSession;这里实现创建不同的服务会话实例
        auto helper = _session_alloc(std::dynamic_pointer_cast<TcpServer>(shared_from_this()), sock);
        auto session = helper->session();
        //把本服务器的配置传递给TcpSession
        session->attachServer(*this);

        //_session_map::emplace肯定能成功
        auto success = _session_map.emplace(helper.get(), helper).second;
        assert(success == true);

        weak_ptr<Session> weak_session = session;
        //会话接收数据事件
        sock->setOnRead([weak_session](const Buffer::Ptr &buf, struct sockaddr *, int) {
            //获取会话强应用
            auto strong_session = weak_session.lock();
            if (strong_session) {
                strong_session->onRecv(buf);
            }
        });

        SessionHelper *ptr = helper.get();
        //会话接收到错误事件
        sock->setOnErr([weak_self, weak_session, ptr](const SockException &err) {
            //在本函数作用域结束时移除会话对象
            //目的是确保移除会话前执行其onError函数
            //同时避免其onError函数抛异常时没有移除会话对象
            onceToken token(nullptr, [&]() {
                //移除掉会话
                auto strong_self = weak_self.lock();
                if (!strong_self) {
                    return;
                }

                assert(strong_self->_poller->isCurrentThread());
                if (!strong_self->_is_on_manager) {
                    //该事件不是onManager时触发的，直接操作map
                    strong_self->_session_map.erase(ptr);
                } else {
                    //遍历map时不能直接删除元素
                    strong_self->_poller->async([weak_self, ptr]() {
                        auto strong_self = weak_self.lock();
                        if (strong_self) {
                            strong_self->_session_map.erase(ptr);
                        }
                    }, false);
                }
            });

            //获取会话强应用
            auto strong_session = weak_session.lock();
            if (strong_session) {
                //触发onError事件回调
                strong_session->onError(err);
            }
        });
    }

private:
    Socket::Ptr onBeforeAcceptConnection_l(const EventPoller::Ptr &poller) {
        return onBeforeAcceptConnection(poller);
    }

    // 接收到客户端连接请求
    void onAcceptConnection_l(const Socket::Ptr &sock) {
        onAcceptConnection(sock);
    }

    template<typename SessionType>
    void start_l(uint16_t port, const std::string &host = "0.0.0.0", uint32_t backlog = 1024) {
        //TcpSession创建器，通过它创建不同类型的服务器
        _session_alloc = [](const TcpServer::Ptr &server, const Socket::Ptr &sock) {
            auto session = std::make_shared<SessionType>(sock);
            session->setOnCreateSocket(server->_on_create_socket);
            return std::make_shared<SessionHelper>(server, session);
        };

        if (!_socket->listen(port, host.c_str(), backlog)) {
            //创建tcp监听失败，可能是由于端口占用或权限问题
            string err = (StrPrinter << "listen on " << host << ":" << port << " failed:" << get_uv_errmsg(true));
            throw std::runtime_error(err);
        }

        //新建一个定时器定时管理这些tcp会话
        weak_ptr<TcpServer> weak_self = std::dynamic_pointer_cast<TcpServer>(shared_from_this());
        _timer = std::make_shared<Timer>(2.0f, [weak_self]() -> bool {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return false;
            }
            strong_self->onManagerSession();
            return true;
        }, _poller);
        InfoL << "TCP Server listening on " << host << ":" << port;
    }

    //定时管理Session
    void onManagerSession() {
        assert(_poller->isCurrentThread());

        onceToken token([&]() {
            _is_on_manager = true;
        }, [&]() {
            _is_on_manager = false;
        });

        for (auto &pr : _session_map) {
            //遍历时，可能触发onErr事件(也会操作_session_map)
            try {
                pr.second->session()->onManager();
            } catch (exception &ex) {
                WarnL << ex.what();
            }
        }
    }

    Socket::Ptr createSocket(){
        return _on_create_socket(_poller);
    }

private:
    bool _cloned = false;
    bool _is_on_manager = false;
    Socket::Ptr _socket;
    std::shared_ptr<Timer> _timer;
    Socket::onCreateSocket _on_create_socket;
    unordered_map<SessionHelper *, SessionHelper::Ptr> _session_map;
    function<SessionHelper::Ptr(const TcpServer::Ptr &server, const Socket::Ptr &)> _session_alloc;
    unordered_map<EventPoller *, Ptr> _cloned_server;
    //对象个数统计
    ObjectStatistic<TcpServer> _statistic;
};

} /* namespace toolkit */
#endif /* TCPSERVER_TCPSERVER_H */
