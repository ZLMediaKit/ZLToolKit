/*
 * Copyright (c) 2021 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TOOLKIT_NETWORK_UDPSERVER_H
#define TOOLKIT_NETWORK_UDPSERVER_H

#include "Network/Server.h"
#include "Network/Session.h"

namespace toolkit {

class UdpServer : public Server {
public:
    using Ptr = std::shared_ptr<UdpServer>;

    explicit UdpServer(const EventPoller::Ptr &poller = nullptr);
    virtual ~UdpServer();

    // 开始监听服务器
    template<typename SessionType>
    void start(uint16_t port, const std::string &host = "0.0.0.0") {
        start_l<SessionType>(port, host);

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
    }

    /**
     * @brief 获取服务器监听端口号, 服务器可以选择监听随机端口
     */
    uint16_t getPort() {
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
    virtual UdpServer::Ptr onCreatServer(const EventPoller::Ptr &poller) {
        return std::make_shared<UdpServer>(poller);
    }

    virtual void cloneFrom(const UdpServer &that);

    /**
     * @brief 应为首次收到来自客户端的数据, 为其创建会话
     */
    virtual void onRead(const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len);

private:
    template<typename SessionType>
    void start_l(uint16_t port, const std::string &host = "0.0.0.0") {
        // Session 创建器, 通过它创建不同类型的服务器
        _session_alloc = [](const UdpServer::Ptr &server, const Socket::Ptr &sock) {
            Session::Ptr session = std::make_shared<SessionType>(sock);
            std::weak_ptr<Server> weak_server = std::static_pointer_cast<Server>(server);
            session->setOnCreateSocket(server->_on_create_socket);
            return std::make_shared<SessionHelper>(weak_server, session);
        };

        if (!_socket->bindUdpSock(port, host.c_str())) {
            // udp 绑定端口失败, 可能是由于端口占用或权限问题
            std::string err = (StrPrinter << "bind " << host << ":" << port << " failed:" << get_uv_errmsg(true));
            throw std::runtime_error(err);
        }

        // 新建一个定时器定时管理这些 udp 会话
        std::weak_ptr<UdpServer> weak_self = std::dynamic_pointer_cast<UdpServer>(shared_from_this());
        _timer = std::make_shared<Timer>(2.0f, [weak_self]() -> bool {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return false;
            }
            strong_self->onManagerSession();
            return true;
        }, _poller);

        InfoL << "UDP Server bind to " << host << ":" << port;
    }

    /**
     * @brief 定时管理 Session, UDP 会话需要根据需要处理超时
     */
    void onManagerSession();

    /**
     * @brief 根据对端信息获取或创建一个会话
     */
    Session::Ptr getOrCreateSession(struct sockaddr *addr, int addr_len);

    /**
     * @brief 创建一个会话, 同时进行必要的设置
     */
    Session::Ptr createSession(const std::string &id, struct sockaddr *addr, int addr_len);

    Socket::Ptr createSocket() { return _on_create_socket(_poller); }

private:
    bool _cloned = false;
    bool _is_on_manager = false;

    Socket::Ptr _socket;
    std::shared_ptr<Timer> _timer;

    Socket::onCreateSocket _on_create_socket;

    std::mutex _session_mutex;
    // peer -> session, 与 session 的 identifier 不同, 此处 peer 仅用于区分不同的对端.
    std::unordered_map<std::string, SessionHelper::Ptr> _session_map;

    std::function<SessionHelper::Ptr(const UdpServer::Ptr&, const Socket::Ptr&)> _session_alloc;
    std::unordered_map<EventPoller *, Ptr> _cloned_server;

    // 对象个数统计
    ObjectStatistic<UdpServer> _statistic;
};

} // namespace toolkit

#endif // TOOLKIT_NETWORK_UDPSERVER_H
