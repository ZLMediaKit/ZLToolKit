/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "TcpServer.h"
#include "Util/uv_errno.h"
#include "Util/onceToken.h"

using namespace std;

namespace toolkit {

INSTANCE_IMP(SessionMap)
StatisticImp(TcpServer)

TcpServer::TcpServer(const EventPoller::Ptr &poller) : Server(poller) {
    setOnCreateSocket(nullptr);
    _socket = createSocket(_poller);
    _socket->setOnBeforeAccept([this](const EventPoller::Ptr &poller) {
        return onBeforeAcceptConnection(poller);
    });
    _socket->setOnAccept([this](Socket::Ptr &sock, shared_ptr<void> &complete) {
        auto ptr = sock->getPoller().get();
        auto server = getServer(ptr);
        ptr->async([server, sock, complete]() {
            //该tcp客户端派发给对应线程的TcpServer服务器
            server->onAcceptConnection(sock);
        });
    });
}

TcpServer::~TcpServer() {
    if (!_parent && _socket->rawFD() != -1) {
        InfoL << "close tcp server [" << _socket->get_local_ip() << "]:" << _socket->get_local_port();
    }
    _timer.reset();
    //先关闭socket监听，防止收到新的连接
    _socket.reset();
    _session_map.clear();
    _cloned_server.clear();
}

uint16_t TcpServer::getPort() {
    if (!_socket) {
        return 0;
    }
    return _socket->get_local_port();
}

void TcpServer::setOnCreateSocket(Socket::onCreateSocket cb) {
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

TcpServer::Ptr TcpServer::onCreatServer(const EventPoller::Ptr &poller) {
    return std::make_shared<TcpServer>(poller);
}

Socket::Ptr TcpServer::onBeforeAcceptConnection(const EventPoller::Ptr &poller) {
    assert(_poller->isCurrentThread());
    //此处改成自定义获取poller对象，防止负载不均衡
    return createSocket(EventPollerPool::Instance().getPoller(false));
}

void TcpServer::cloneFrom(const TcpServer &that) {
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
    _parent = &that;
}

// 接收到客户端连接请求
void TcpServer::onAcceptConnection(const Socket::Ptr &sock) {
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
        if (!strong_session) {
            return;
        }
        try {
            strong_session->onRecv(buf);
        } catch (SockException &ex) {
            strong_session->shutdown(ex);
        } catch (exception &ex) {
            strong_session->shutdown(SockException(Err_shutdown, ex.what()));
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

void TcpServer::start_l(uint16_t port, const std::string &host, uint32_t backlog) {
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

    InfoL << "TCP Server listening on [" << host << "]:" << port;
}

void TcpServer::onManagerSession() {
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

Socket::Ptr TcpServer::createSocket(const EventPoller::Ptr &poller) {
    return _on_create_socket(poller);
}

TcpServer::Ptr TcpServer::getServer(const EventPoller *poller) const {
    auto &ref = _parent ? _parent->_cloned_server : _cloned_server;
    auto it = ref.find(poller);
    if (it != ref.end()) {
        //派发到cloned server
        return it->second;
    }
    //派发到parent server
    return static_pointer_cast<TcpServer>(_parent ? const_cast<TcpServer *>(_parent)->shared_from_this() :
                                          const_cast<TcpServer *>(this)->shared_from_this());
}


} /* namespace toolkit */

