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

template<typename SessionType>
class UdpServer : public Server {
public:
    using Ptr = std::shared_ptr<UdpServer<SessionType>>;

    using PeerIdType = uint64_t;

    explicit UdpServer(const EventPoller::Ptr &poller = nullptr) : Server(poller) {
        setOnCreateSocket(nullptr);
        _socket = createSocket();
        _socket->setOnRead([this](const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
            onRead(buf, addr, addr_len);
        });
    }
    virtual ~UdpServer() {
        if (!_cloned && _socket->rawFD() != -1) {
            InfoL << "close udp server " << _socket->get_local_ip() << ":" << _socket->get_local_port();
        }

        _timer.reset();
        // 先关闭 socket 监听, 防止收到新的连接
        _socket.reset();

        // 先释放 cloned server,
        _cloned_server.clear();

        if (!_cloned) {
            std::lock_guard<std::mutex> lock(_session_mutex);
            _session_map.clear();
        }
    }

    // 开始监听服务器
    void start(uint16_t port, const std::string &host = "0.0.0.0") {
        start_l(port, host);

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

    virtual void cloneFrom(const UdpServer &that) {
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

        // 各 server 管理各自的会话.
        std::weak_ptr<UdpServer> weak_self = std::dynamic_pointer_cast<UdpServer>(shared_from_this());
        _timer = std::make_shared<Timer>(2.0f, [weak_self]() -> bool {
            if (auto strong_self = weak_self.lock()) {
                strong_self->onManagerSession();
                return true;
            }
            return false;
        }, _poller);

        // clone properties
        this->mINI::operator=(that);

        _cloned = true;
    }

    /**
     * @brief 应为首次收到来自客户端的数据, 为其创建会话
     */
    virtual void onRead(const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
        const auto id = makeSockId(addr, addr_len);
        onRead_l(id, buf, addr, addr_len);
    }

private:
    void start_l(uint16_t port, const std::string &host = "0.0.0.0") {
        // Session 创建器, 通过它创建不同类型的服务器
        _session_alloc = [](const UdpServer::Ptr &server, const Socket::Ptr &sock) {
            Session::Ptr session = std::make_shared<SessionType>(sock);
            std::weak_ptr<Server> weak_server = std::static_pointer_cast<Server>(server);
            session->setOnCreateSocket(server->_on_create_socket);
            return std::make_shared<SessionHelper>(weak_server, session);
        };

        std::weak_ptr<UdpServer> weak_self = std::dynamic_pointer_cast<UdpServer>(shared_from_this());

        // 是可能在退出的时候收到数据的.
        _socket->setOnRead([weak_self](const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
            if (auto strong_self = weak_self.lock()) {
                strong_self->onRead(buf, addr, addr_len);
            }
        });

        if (!_socket->bindUdpSock(port, host.c_str())) {
            // udp 绑定端口失败, 可能是由于端口占用或权限问题
            std::string err = (StrPrinter << "bind " << host << ":" << port << " failed:" << get_uv_errmsg(true));
            throw std::runtime_error(err);
        }

        // 新建一个定时器定时管理这些 udp 会话, 仅在主服务中进行管理.
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
     * @brief 定时管理 Session
     * @note UDP 会话需要处理超时无数据的情况
     */
    void onManagerSession() {
        assert(_poller->isCurrentThread());

        std::unordered_map<PeerIdType, SessionHelper::Ptr> session_map;
        {
            std::lock_guard<std::mutex> lock(_session_mutex);
            session_map = _session_map;
        }

        for (auto &pr : session_map) {
            // 遍历时, 可能触发 onErr 事件(也会操作 _session_map)
            try {
                // 各 server 管理各自的会话.
                auto session = pr.second->session();
                if (session->getPoller()->isCurrentThread()) {
                    // UDP 会话需要处理超时
                    session->onManager();
                }
            } catch (exception &ex) {
                WarnL << ex.what();
            }
        }
    }

    void onRead_l(const PeerIdType &id, const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
        if (auto session = getOrCreateSession(id, addr, addr_len)) {
            std::weak_ptr<Session> weak_session = session;
            // 应不需要切换, Linux 下暂未发现已绑定的会话收到其他已创建会话的数据
            session->async([weak_session, buf]() {
                if (auto strong_session = weak_session.lock()) {
                    strong_session->onRecv(buf);
                }
            });
        }
    }

    /**
     * @brief 根据对端信息获取或创建一个会话
     */
    Session::Ptr getOrCreateSession(const PeerIdType &id, struct sockaddr *addr, int addr_len) {
        std::lock_guard<std::mutex> lock(_session_mutex);
        auto it = _session_map.find(id);
        if (it != _session_map.end())
            return it->second->session();

        // under lock
        return createSession(id, addr, addr_len);
    }

    /**
     * @brief 创建一个会话, 同时进行必要的设置
     */
    Session::Ptr createSession(const PeerIdType &id, struct sockaddr *addr, int addr_len) {
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

            // 不支持绑定 socket 或收到新连接, 重新查找, 实测主要是有新连接
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

                // 可以直接删除, 管理时会拷贝 session_map
                std::lock_guard<std::mutex> lock(_session_mutex);
                _session_map.erase(id);
            });

            // 获取会话强应用
            if (auto strong_session = weak_session.lock()) {
                // 触发 onError 事件回调
                strong_session->onError(err);
            }
        });

        socket->bindUdpSock(_socket->get_local_port(), _socket->get_local_ip());
        SockUtil::connectUdpSock(socket->rawFD(), addr, addr_len);
        // 在 connect peer 后再取消绑定关系, 避免在 server 的 socket 或其他 cloned server 中收到后续数据包.
        SockUtil::dissolveUdpSock(_socket->rawFD());
        _session_map.emplace(id, std::move(helper));
        return session;
    }

    Socket::Ptr createSocket() { return _on_create_socket(_poller); }

    static PeerIdType makeSockId(sockaddr *addr, int) {
        return (uint64_t)(((struct sockaddr_in *) addr)->sin_addr.s_addr) << 16 | ((struct sockaddr_in *) addr)->sin_port;
    }

private:
    bool _cloned = false;

    Socket::Ptr _socket;
    std::shared_ptr<Timer> _timer;

    Socket::onCreateSocket _on_create_socket;

    // 暂时未发现已绑定的 socket 的数据从其他 socket 接收的情况, 不需要静态
    static std::mutex _session_mutex;
    // peer -> session, 与 session 的 identifier 不同, 此处 peer 仅用于区分不同的对端.
    static std::unordered_map<PeerIdType, SessionHelper::Ptr> _session_map;

    std::function<SessionHelper::Ptr(const UdpServer::Ptr&, const Socket::Ptr&)> _session_alloc;
    std::unordered_map<EventPoller *, Ptr> _cloned_server;

    // 对象个数统计
    ObjectStatistic<UdpServer> _statistic;
};

template<typename SessionType>
std::mutex UdpServer<SessionType>::_session_mutex;

template<typename SessionType>
std::unordered_map<typename UdpServer<SessionType>::PeerIdType, SessionHelper::Ptr>
UdpServer<SessionType>::_session_map;

} // namespace toolkit

#endif // TOOLKIT_NETWORK_UDPSERVER_H
