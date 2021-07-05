/*
 * Copyright (c) 2021 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Network/UdpServer.h"

namespace toolkit {

static UdpServer::PeerIdType makeSockId(sockaddr *addr, int) {
    return ((uint64_t)((struct sockaddr_in *) addr)->sin_addr.s_addr) << 16 | ((struct sockaddr_in *) addr)->sin_port;
}

UdpServer::UdpServer(const EventPoller::Ptr &poller) : Server(poller) {
    setOnCreateSocket(nullptr);
    _socket = createSocket(_poller);
    _socket->setOnRead([this](const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
        onRead(buf, addr, addr_len);
    });
}

UdpServer::~UdpServer() {
    if (!_cloned && _socket->rawFD() != -1) {
        InfoL << "close udp server " << _socket->get_local_ip() << ":" << _socket->get_local_port();
    }
    _timer.reset();
    _socket.reset();
    _cloned_server.clear();
    if (!_cloned) {
        lock_guard<std::recursive_mutex> lck(*_session_mutex);
        _session_map->clear();
    }
}

void UdpServer::start_l(uint16_t port, const std::string &host) {
    //主server才创建session map，其他cloned server共享之
    _session_mutex = std::make_shared<std::recursive_mutex>();
    _session_map = std::make_shared<std::unordered_map<PeerIdType, SessionHelper::Ptr> >();

    if (!_socket->bindUdpSock(port, host.c_str())) {
        // udp 绑定端口失败, 可能是由于端口占用或权限问题
        std::string err = (StrPrinter << "bind " << host << ":" << port << " failed:" << get_uv_errmsg(true));
        throw std::runtime_error(err);
    }

    // 新建一个定时器定时管理这些 udp 会话,这些对象只由主server做超时管理，cloned server不管理
    std::weak_ptr<UdpServer> weak_self = std::dynamic_pointer_cast<UdpServer>(shared_from_this());
    _timer = std::make_shared<Timer>(2.0f, [weak_self]() -> bool {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return false;
        }
        strong_self->onManagerSession();
        return true;
    }, _poller);

    //clone server至不同线程，让udp server支持多线程
    EventPollerPool::Instance().for_each([&](const TaskExecutor::Ptr &executor) {
        auto poller = std::dynamic_pointer_cast<EventPoller>(executor);
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

    InfoL << "UDP Server bind to " << host << ":" << port;
}

UdpServer::Ptr UdpServer::onCreatServer(const EventPoller::Ptr &poller) {
    return std::make_shared<UdpServer>(poller);
}

void UdpServer::cloneFrom(const UdpServer &that) {
    if (!that._socket) {
        throw std::invalid_argument("UdpServer::cloneFrom other with NULL socket!");
    }
    // clone callbacks
    _on_create_socket = that._on_create_socket;
    _session_alloc = that._session_alloc;
    _session_mutex = that._session_mutex;
    _session_map = that._session_map;
    // clone udp socket
    _socket->bindUdpSock(that._socket->get_local_port(), that._socket->get_local_ip());
    // clone properties
    this->mINI::operator=(that);
    _cloned = true;
}

void UdpServer::onRead(const Buffer::Ptr &buf, sockaddr *addr, int addr_len) {
    const auto id = makeSockId(addr, addr_len);
    onRead_l(true, id, buf, addr, addr_len);
}

void UdpServer::onRead_l(bool is_server_fd, const UdpServer::PeerIdType &id, const Buffer::Ptr &buf, sockaddr *addr, int addr_len) {
    // udp server fd收到数据时触发此函数；大部分情况下数据应该在peer fd触发，此函数应该不是热点函数
    bool is_new = false;
    if (auto &session = getOrCreateSession(id, addr, addr_len, is_new)) {
        std::weak_ptr<Session> weak_session = session;
        //数据可能漂移到其他线程，所以此处尝试切换线程(通常不需要)
        session->async([weak_session, buf]() {
            if (auto strong_session = weak_session.lock()) {
                strong_session->onRecv(buf);
            }
        });
#if !defined(NDEBUG)
        if (!session->getPoller()->isCurrentThread()) {
            WarnL << "udp packet incoming from other thread";
        }
        if (!is_new) {
            TraceL << "udp packet incoming from " << (is_server_fd ? "server fd" : "other peer fd");
        }
#endif
    }
}

void UdpServer::onManagerSession() {
    decltype(_session_map) copy_map;
    {
        std::lock_guard<std::recursive_mutex> lock(*_session_mutex);
        //拷贝map，防止遍历时移除对象
        copy_map = std::make_shared<std::unordered_map<PeerIdType, SessionHelper::Ptr> >(*_session_map);
    }
    EventPollerPool::Instance().for_each([copy_map](const TaskExecutor::Ptr &executor) {
        auto poller = std::dynamic_pointer_cast<EventPoller>(executor);
        if (!poller) {
            return;
        }
        poller->async([copy_map]() {
            for (auto &pr : *copy_map) {
                auto &session = pr.second->session();
                if (!session->getPoller()->isCurrentThread()) {
                    //该session不归属该poller管理
                    continue;
                }
                try {
                    // UDP 会话需要处理超时
                    session->onManager();
                } catch (exception &ex) {
                    WarnL << ex.what();
                }
            }
        });
    });
}

const Session::Ptr& UdpServer::getOrCreateSession(const UdpServer::PeerIdType &id, sockaddr *addr, int addr_len, bool &is_new) {
    {
        //减小临界区
        std::lock_guard<std::recursive_mutex> lock(*_session_mutex);
        auto it = _session_map->find(id);
        if (it != _session_map->end()) {
            return it->second->session();
        }
    }
    is_new = true;
    return createSession(id, addr, addr_len);
}

const Session::Ptr& UdpServer::createSession(const PeerIdType &id, sockaddr *addr, int addr_len) {
    auto socket = createSocket(_poller);

    socket->bindUdpSock(_socket->get_local_port(), _socket->get_local_ip());
    socket->bindPeerAddr(addr, addr_len);
    //在connect peer后再取消绑定关系, 避免在 server 的 socket 或其他cloned server中收到后续数据包.
    SockUtil::dissolveUdpSock(_socket->rawFD());

    auto server = std::dynamic_pointer_cast<UdpServer>(shared_from_this());
    auto helper = _session_alloc(server, socket);
    auto session = helper->session();
    // 把本服务器的配置传递给 Session
    session->attachServer(*this);

    std::weak_ptr<UdpServer> weak_self = server;
    std::weak_ptr<Session> weak_session = session;
    socket->setOnRead([weak_self, weak_session, id](const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }

        //快速判断是否为本会话的的数据, 通常应该成立
        if (id == makeSockId(addr, addr_len)) {
            if (auto strong_session = weak_session.lock()) {
                strong_session->onRecv(buf);
            }
            return;
        }

        //收到非本peer fd的数据，让server去派发此数据到合适的session对象
        strong_self->onRead_l(false, id, buf, addr, addr_len);
    });
    socket->setOnErr([weak_self, weak_session, id](const SockException &err) {
        // 在本函数作用域结束时移除会话对象
        // 目的是确保移除会话前执行其 onError 函数
        // 同时避免其 onError 函数抛异常时没有移除会话对象
        onceToken token(nullptr, [&]() {
            // 移除掉会话
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }
            assert(strong_self->_poller->isCurrentThread());
            //从共享map中移除本session对象
            lock_guard<std::recursive_mutex> lck(*strong_self->_session_mutex);
            strong_self->_session_map->erase(id);
        });

        // 获取会话强应用
        if (auto strong_session = weak_session.lock()) {
            // 触发 onError 事件回调
            strong_session->onError(err);
        }
    });

    lock_guard<std::recursive_mutex> lck(*_session_mutex);
    auto pr = _session_map->emplace(id, std::move(helper));
    assert(pr.second);
    return pr.first->second->session();
}

void UdpServer::setOnCreateSocket(Socket::onCreateSocket cb) {
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

uint16_t UdpServer::getPort() {
    if (!_socket) {
        return 0;
    }
    return _socket->get_local_port();
}

Socket::Ptr UdpServer::createSocket(const EventPoller::Ptr &poller) {
    return _on_create_socket(poller);
}


StatisticImp(UdpServer)

} // namespace toolkit
