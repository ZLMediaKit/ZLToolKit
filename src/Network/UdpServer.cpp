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

static std::string makeSockId(sockaddr* addr, int) {
    std::stringstream ss;
    ss << SockUtil::inet_ntoa(((struct sockaddr_in *) addr)->sin_addr) << ":" << ::ntohs(((struct sockaddr_in *) addr)->sin_port);
    return ss.str();
}

UdpServer::UdpServer(const EventPoller::Ptr& poller)
    : Server(poller) {
    setOnCreateSocket(nullptr);
    _socket = createSocket();
    _socket->setOnRead(std::bind(&UdpServer::onRead, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

UdpServer::~UdpServer() {
    if (!_cloned && _socket->rawFD() != -1) {
        InfoL << "close udp server " << _socket->get_local_ip() << ":" << _socket->get_local_port();
    }

    _timer.reset();

    // 先关闭 socket 监听, 防止收到新的连接
    _socket.reset();
    _session_map.clear();
    _cloned_server.clear();
}

void UdpServer::cloneFrom(const UdpServer& that) {
    if (!that._socket) {
        throw std::invalid_argument("UdpServer::cloneFrom other with NULL socket!");
    }

    // clone callbacks
    _on_create_socket = that._on_create_socket;
    _session_alloc = that._session_alloc;

    // clone socket info
    auto addr = that._socket->get_local_ip();
    auto port = that._socket->get_local_port();
    _socket->bindUdpSock(port, addr);
    std::weak_ptr<UdpServer> weak_self = std::dynamic_pointer_cast<UdpServer>(shared_from_this());
    _timer = std::make_shared<Timer>(2.0f, [weak_self]() -> bool {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return false;
        }
        strong_self->onManagerSession();
        return true;
    }, _poller);

    // clone properties
    this->mINI::operator=(that);

    _cloned = true;
}

void UdpServer::onRead(const Buffer::Ptr &buf, sockaddr *addr, int addr_len) {
    if (auto session = getOrCreateSession(addr, addr_len)) {
        session->onRecv(buf);
    }
}

void UdpServer::onManagerSession() {
    assert(_poller->isCurrentThread());

    onceToken token([&]() {
        _is_on_manager = true;
    }, [&]() {
        _is_on_manager = false;
    });

    for (auto &pr : _session_map) {
        // 遍历时, 可能触发 onErr 事件(也会操作 _session_map)
        try {
            // UDP 会话需要处理超时
            pr.second->session()->onManager();
        } catch (exception &ex) {
            WarnL << ex.what();
        }
    }
}

Session::Ptr UdpServer::getOrCreateSession(sockaddr* addr, int addr_len) {
    const auto id = makeSockId(addr, addr_len);

    std::lock_guard<std::mutex> lock(_session_mutex);
    auto it = _session_map.find(id);
    if (it != _session_map.end())
        return it->second->session();

    // under lock
    return createSession(id, addr, addr_len);
}

Session::Ptr UdpServer::createSession(const string& id, sockaddr* addr, int addr_len) {
    auto socket = createSocket();
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

        // 快速判断是否为本会话的的数据, 通常应该成立
        if (id == makeSockId(addr, addr_len)) {
            if (auto strong_session = weak_session.lock()) {
                strong_session->onRecv(buf);
            }
            return;
        }

        // 不支持绑定 socket, 重新查找
        strong_self->onRead(buf, addr, addr_len);
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
            if (!strong_self->_is_on_manager) {
                // 该事件不是 onManager 时触发的, 直接操作 map
                strong_self->_session_map.erase(id);
            } else {
                // 遍历 map 时不能直接删除元素
                strong_self->_poller->async([weak_self, id]() {
                    auto strong_self = weak_self.lock();
                    if (strong_self) {
                        strong_self->_session_map.erase(id);
                    }
                }, false);
            }
        });

        // 获取会话强应用
        if (auto strong_session = weak_session.lock()) {
            // 触发 onError 事件回调
            strong_session->onError(err);
        }
    });

    // 取消绑定关系, 避免在 server 的 socket 中收到后续数据包.
    SockUtil::dissolveUdpSock(_socket->rawFD());
    socket->bindUdpSock(_socket->get_local_port(), _socket->get_local_ip());
    SockUtil::connectUdpSock(socket->rawFD(), addr, addr_len);

    _session_map.emplace(id, helper);

    return session;
}

StatisticImp(UdpServer)

} // namespace toolkit
