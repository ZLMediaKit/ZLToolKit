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
    _multi_poller = !poller;
    setOnCreateSocket(nullptr);
}

void TcpServer::setupEvent() {
    _socket = createSocket(_poller);
    weak_ptr<TcpServer> weak_self = std::static_pointer_cast<TcpServer>(shared_from_this());
    _socket->setOnBeforeAccept([weak_self](const EventPoller::Ptr &poller) -> Socket::Ptr {
        if (auto strong_self = weak_self.lock()) {
            return strong_self->onBeforeAcceptConnection(poller);
        }
        return nullptr;
    });
    _socket->setOnAccept([weak_self](Socket::Ptr &sock, shared_ptr<void> &complete) {
        if (auto strong_self = weak_self.lock()) {
            auto ptr = sock->getPoller().get();
            auto server = strong_self->getServer(ptr);
            ptr->async([server, sock, complete]() {
                //该tcp客户端派发给对应线程的TcpServer服务器  [AUTO-TRANSLATED:662b882f]
                //This TCP client is dispatched to the corresponding thread of the TcpServer server
                server->onAcceptConnection(sock);
            });
        }
    });
}

TcpServer::~TcpServer() {
    if (_main_server && _socket && _socket->rawFD() != -1) {
        InfoL << "Close tcp server [" << _socket->get_local_ip() << "]: " << _socket->get_local_port();
    }
    _timer.reset();
    //先关闭socket监听，防止收到新的连接  [AUTO-TRANSLATED:cd65064f]
    //First close the socket listening to prevent receiving new connections
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
    return Ptr(new TcpServer(poller), [poller](TcpServer *ptr) { poller->async([ptr]() { delete ptr; }); });
}

Socket::Ptr TcpServer::onBeforeAcceptConnection(const EventPoller::Ptr &poller) {
    assert(_poller->isCurrentThread());
    //此处改成自定义获取poller对象，防止负载不均衡  [AUTO-TRANSLATED:16c66457]
    //Modify this to a custom way of getting the poller object to prevent load imbalance
    return createSocket(_multi_poller ? EventPollerPool::Instance().getPoller(false) : _poller);
}

void TcpServer::cloneFrom(const TcpServer &that) {
    if (!that._socket) {
        throw std::invalid_argument("TcpServer::cloneFrom other with null socket");
    }
    setupEvent();
    _main_server = false;
    _on_create_socket = that._on_create_socket;
    _session_alloc = that._session_alloc;
    weak_ptr<TcpServer> weak_self = std::static_pointer_cast<TcpServer>(shared_from_this());
    _timer = std::make_shared<Timer>(2.0f, [weak_self]() -> bool {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return false;
        }
        strong_self->onManagerSession();
        return true;
    }, _poller);
    this->mINI::operator=(that);
    _parent = static_pointer_cast<TcpServer>(const_cast<TcpServer &>(that).shared_from_this());
}

// 接收到客户端连接请求  [AUTO-TRANSLATED:8a67b72a]
//Received a client connection request
Session::Ptr TcpServer::onAcceptConnection(const Socket::Ptr &sock) {
    assert(_poller->isCurrentThread());
    weak_ptr<TcpServer> weak_self = std::static_pointer_cast<TcpServer>(shared_from_this());
    //创建一个Session;这里实现创建不同的服务会话实例  [AUTO-TRANSLATED:9ed745be]
    //Create a Session; here implement creating different service session instances
    auto helper = _session_alloc(std::static_pointer_cast<TcpServer>(shared_from_this()), sock);
    auto session = helper->session();
    //把本服务器的配置传递给Session  [AUTO-TRANSLATED:e3711484]
    //Pass the configuration of this server to the Session
    session->attachServer(*this);

    //_session_map::emplace肯定能成功  [AUTO-TRANSLATED:09d4aef7]
    //_session_map::emplace will definitely succeed
    auto success = _session_map.emplace(helper.get(), helper).second;
    assert(success == true);

    weak_ptr<Session> weak_session = session;
    //会话接收数据事件  [AUTO-TRANSLATED:f3f4cbbb]
    //Session receives data event
    sock->setOnRead([weak_session](const Buffer::Ptr &buf, struct sockaddr *, int) {
        //获取会话强应用  [AUTO-TRANSLATED:187497e6]
        //Get the strong application of the session
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
    auto cls = ptr->className();
    //会话接收到错误事件  [AUTO-TRANSLATED:b000e868]
    //Session receives an error event
    sock->setOnErr([weak_self, weak_session, ptr, cls](const SockException &err) {
        //在本函数作用域结束时移除会话对象  [AUTO-TRANSLATED:5c4433b8]
        //Remove the session object when the function scope ends
        //目的是确保移除会话前执行其onError函数  [AUTO-TRANSLATED:1e6c65df]
        //The purpose is to ensure that the onError function is executed before removing the session
        //同时避免其onError函数抛异常时没有移除会话对象  [AUTO-TRANSLATED:6d541cbd]
        //And avoid not removing the session object when the onError function throws an exception
        onceToken token(nullptr, [&]() {
            //移除掉会话  [AUTO-TRANSLATED:e7c27790]
            //Remove the session
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }

            assert(strong_self->_poller->isCurrentThread());
            if (!strong_self->_is_on_manager) {
                //该事件不是onManager时触发的，直接操作map  [AUTO-TRANSLATED:d90ee039]
                //This event is not triggered by onManager, directly operate on the map
                strong_self->_session_map.erase(ptr);
            } else {
                //遍历map时不能直接删除元素  [AUTO-TRANSLATED:0f00040c]
                //Cannot directly delete elements when traversing the map
                strong_self->_poller->async([weak_self, ptr]() {
                    auto strong_self = weak_self.lock();
                    if (strong_self) {
                        strong_self->_session_map.erase(ptr);
                    }
                }, false);
            }
        });

        //获取会话强应用  [AUTO-TRANSLATED:187497e6]
        //Get the strong reference of the session
        auto strong_session = weak_session.lock();
        if (strong_session) {
            //触发onError事件回调  [AUTO-TRANSLATED:825d16df]
            //Trigger the onError event callback
            TraceP(strong_session) << cls << " on err: " << err;
            strong_session->onError(err);
        }
    });
    return session;
}

void TcpServer::start_l(uint16_t port, const std::string &host, uint32_t backlog) {
    setupEvent();

    //新建一个定时器定时管理这些tcp会话  [AUTO-TRANSLATED:ef859bd7]
    //Create a new timer to manage these TCP sessions periodically
    weak_ptr<TcpServer> weak_self = std::static_pointer_cast<TcpServer>(shared_from_this());
    _timer = std::make_shared<Timer>(2.0f, [weak_self]() -> bool {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return false;
        }
        strong_self->onManagerSession();
        return true;
    }, _poller);

    if (_multi_poller) {
        EventPollerPool::Instance().for_each([&](const TaskExecutor::Ptr &executor) {
            EventPoller::Ptr poller = static_pointer_cast<EventPoller>(executor);
            if (poller == _poller) {
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

    if (!_socket->listen(port, host.c_str(), backlog)) {
        // 创建tcp监听失败，可能是由于端口占用或权限问题  [AUTO-TRANSLATED:88ebdefc]
        //TCP listener creation failed, possibly due to port occupation or permission issues
        string err = (StrPrinter << "Listen on " << host << " " << port << " failed: " << get_uv_errmsg(true));
        throw std::runtime_error(err);
    }
    for (auto &pr: _cloned_server) {
        // 启动子Server  [AUTO-TRANSLATED:1820131c]
        //Start the child Server
        pr.second->_socket->cloneSocket(*_socket);
    }

    InfoL << "TCP server listening on [" << host << "]: " << port;
}

void TcpServer::onManagerSession() {
    assert(_poller->isCurrentThread());

    onceToken token([&]() {
        _is_on_manager = true;
    }, [&]() {
        _is_on_manager = false;
    });

    for (auto &pr : _session_map) {
        //遍历时，可能触发onErr事件(也会操作_session_map)  [AUTO-TRANSLATED:7760b80d]
        //When traversing, the onErr event may be triggered (also operates on _session_map)
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
    auto parent = _parent.lock();
    auto &ref = parent ? parent->_cloned_server : _cloned_server;
    auto it = ref.find(poller);
    if (it != ref.end()) {
        //派发到cloned server  [AUTO-TRANSLATED:8765ab56]
        //Dispatch to the cloned server
        return it->second;
    }
    //派发到parent server  [AUTO-TRANSLATED:4cf34169]
    //Dispatch to the parent server
    return static_pointer_cast<TcpServer>(parent ? parent : const_cast<TcpServer *>(this)->shared_from_this());
}

Session::Ptr TcpServer::createSession(const Socket::Ptr &sock) {
    return getServer(sock->getPoller().get())->onAcceptConnection(sock);
}

} /* namespace toolkit */

