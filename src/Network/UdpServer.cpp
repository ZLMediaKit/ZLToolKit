/*
 * Copyright (c) 2021 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Util/uv_errno.h"
#include "Util/onceToken.h"
#include "UdpServer.h"

using namespace std;

namespace toolkit {

static const uint8_t s_in6_addr_maped[]
    = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00 };

static constexpr auto kUdpDelayCloseMS = 3 * 1000;

static UdpServer::PeerIdType makeSockId(sockaddr *addr, int) {
    UdpServer::PeerIdType ret;
    switch (addr->sa_family) {
        case AF_INET : {
            ret.resize(18);
            ret[0] = ((struct sockaddr_in *) addr)->sin_port >> 8;
            ret[1] = ((struct sockaddr_in *) addr)->sin_port & 0xFF;
            //ipv4地址统一转换为ipv6方式处理
            memcpy(&ret[2], &s_in6_addr_maped, 12);
            memcpy(&ret[14], &(((struct sockaddr_in *) addr)->sin_addr), 4);
            return ret;
        }
        case AF_INET6 : {
            ret.resize(18);
            ret[0] = ((struct sockaddr_in6 *) addr)->sin6_port >> 8;
            ret[1] = ((struct sockaddr_in6 *) addr)->sin6_port & 0xFF;
            memcpy(&ret[2], &(((struct sockaddr_in6 *)addr)->sin6_addr), 16);
            return ret;
        }
        default: assert(0); return "";
    }
}

UdpServer::UdpServer(const EventPoller::Ptr &poller) : Server(poller) {
    setOnCreateSocket(nullptr);
}

void UdpServer::setupEvent() {
    _socket = createSocket(_poller);
    std::weak_ptr<UdpServer> weak_self = std::static_pointer_cast<UdpServer>(shared_from_this());
    _socket->setOnRead([weak_self](const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
        if (auto strong_self = weak_self.lock()) {
            strong_self->onRead(buf, addr, addr_len);
        }
    });
}

UdpServer::~UdpServer() {
    if (!_cloned && _socket && _socket->rawFD() != -1) {
        InfoL << "Close udp server [" << _socket->get_local_ip() << "]: " << _socket->get_local_port();
    }
    _timer.reset();
    _socket.reset();
    _cloned_server.clear();
    if (!_cloned && _session_mutex && _session_map) {
        lock_guard<std::recursive_mutex> lck(*_session_mutex);
        _session_map->clear();
    }
}

void UdpServer::start_l(uint16_t port, const std::string &host) {
    setupEvent();
    //主server才创建session map，其他cloned server共享之
    _session_mutex = std::make_shared<std::recursive_mutex>();
    _session_map = std::make_shared<std::unordered_map<PeerIdType, SessionHelper::Ptr> >();

    // 新建一个定时器定时管理这些 udp 会话,这些对象只由主server做超时管理，cloned server不管理
    std::weak_ptr<UdpServer> weak_self = std::static_pointer_cast<UdpServer>(shared_from_this());
    _timer = std::make_shared<Timer>(2.0f, [weak_self]() -> bool {
        if (auto strong_self = weak_self.lock()) {
            strong_self->onManagerSession();
            return true;
        }
        return false;
    }, _poller);

    //clone server至不同线程，让udp server支持多线程
    EventPollerPool::Instance().for_each([&](const TaskExecutor::Ptr &executor) {
        auto poller = std::static_pointer_cast<EventPoller>(executor);
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

    if (!_socket->bindUdpSock(port, host.c_str())) {
        // udp 绑定端口失败, 可能是由于端口占用或权限问题
        std::string err = (StrPrinter << "Bind udp socket on " << host << " " << port << " failed: " << get_uv_errmsg(true));
        throw std::runtime_error(err);
    }

    for (auto &pr: _cloned_server) {
        // 启动子Server
#if 0
        pr.second->_socket->cloneSocket(*_socket);
#else
        // 实验发现cloneSocket方式虽然可以节省fd资源，但是在某些系统上线程漂移问题更严重
        pr.second->_socket->bindUdpSock(_socket->get_local_port(), _socket->get_local_ip());
#endif
    }
    InfoL << "UDP server bind to [" << host << "]: " << port;
}

UdpServer::Ptr UdpServer::onCreatServer(const EventPoller::Ptr &poller) {
    return std::make_shared<UdpServer>(poller);
}

void UdpServer::cloneFrom(const UdpServer &that) {
    if (!that._socket) {
        throw std::invalid_argument("UdpServer::cloneFrom other with null socket");
    }
    setupEvent();
    _cloned = true;
    // clone callbacks
    _on_create_socket = that._on_create_socket;
    _session_alloc = that._session_alloc;
    _session_mutex = that._session_mutex;
    _session_map = that._session_map;
    // clone properties
    this->mINI::operator=(that);
}

void UdpServer::onRead(const Buffer::Ptr &buf, sockaddr *addr, int addr_len) {
    const auto id = makeSockId(addr, addr_len);
    onRead_l(true, id, buf, addr, addr_len);
}

static void emitSessionRecv(const SessionHelper::Ptr &helper, const Buffer::Ptr &buf) {
    if (!helper->enable) {
        // 延时销毁中
        return;
    }
    try {
        helper->session()->onRecv(buf);
    } catch (SockException &ex) {
        helper->session()->shutdown(ex);
    } catch (exception &ex) {
        helper->session()->shutdown(SockException(Err_shutdown, ex.what()));
    }
}

void UdpServer::onRead_l(bool is_server_fd, const UdpServer::PeerIdType &id, const Buffer::Ptr &buf, sockaddr *addr, int addr_len) {
    // udp server fd收到数据时触发此函数；大部分情况下数据应该在peer fd触发，此函数应该不是热点函数
    bool is_new = false;
    if (auto helper = getOrCreateSession(id, buf, addr, addr_len, is_new)) {
        if (helper->session()->getPoller()->isCurrentThread()) {
            //当前线程收到数据，直接处理数据
            emitSessionRecv(helper, buf);
        } else {
            //数据漂移到其他线程，需要先切换线程
            WarnL << "UDP packet incoming from other thread";
            std::weak_ptr<SessionHelper> weak_helper = helper;
            //由于socket读buffer是该线程上所有socket共享复用的，所以不能跨线程使用，必须先拷贝一下
            auto cacheable_buf = std::make_shared<BufferString>(buf->toString());
            helper->session()->async([weak_helper, cacheable_buf]() {
                if (auto strong_helper = weak_helper.lock()) {
                    emitSessionRecv(strong_helper, cacheable_buf);
                }
            });
        }

#if !defined(NDEBUG)
        if (!is_new) {
            TraceL << "UDP packet incoming from " << (is_server_fd ? "server fd" : "other peer fd");
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
        auto poller = std::static_pointer_cast<EventPoller>(executor);
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
                    WarnL << "Exception occurred when emit onManager: " << ex.what();
                }
            }
        });
    });
}

SessionHelper::Ptr UdpServer::getOrCreateSession(const UdpServer::PeerIdType &id, const Buffer::Ptr &buf, sockaddr *addr, int addr_len, bool &is_new) {
    {
        //减小临界区
        std::lock_guard<std::recursive_mutex> lock(*_session_mutex);
        auto it = _session_map->find(id);
        if (it != _session_map->end()) {
            return it->second;
        }
    }
    is_new = true;
    return createSession(id, buf, addr, addr_len);
}

SessionHelper::Ptr UdpServer::createSession(const PeerIdType &id, const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
    // 此处改成自定义获取poller对象，防止负载不均衡
    auto socket = createSocket(EventPollerPool::Instance().getPoller(false), buf, addr, addr_len);
    if (!socket) {
        //创建socket失败，本次onRead事件收到的数据直接丢弃
        return nullptr;
    }

    auto addr_str = string((char *) addr, addr_len);
    std::weak_ptr<UdpServer> weak_self = std::static_pointer_cast<UdpServer>(shared_from_this());
    auto helper_creator = [this, weak_self, socket, addr_str, id]() -> SessionHelper::Ptr {
        auto server = weak_self.lock();
        if (!server) {
            return nullptr;
        }

        //如果已经创建该客户端对应的UdpSession类，那么直接返回
        lock_guard<std::recursive_mutex> lck(*_session_mutex);
        auto it = _session_map->find(id);
        if (it != _session_map->end()) {
            return it->second;
        }

        assert(_socket);
        socket->bindUdpSock(_socket->get_local_port(), _socket->get_local_ip());
        socket->bindPeerAddr((struct sockaddr *) addr_str.data(), addr_str.size());

        auto helper = _session_alloc(server, socket);
        // 把本服务器的配置传递给 Session
        helper->session()->attachServer(*this);

        std::weak_ptr<SessionHelper> weak_helper = helper;
        socket->setOnRead([weak_self, weak_helper, id](const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }

            //快速判断是否为本会话的的数据, 通常应该成立
            if (id == makeSockId(addr, addr_len)) {
                if (auto strong_helper = weak_helper.lock()) {
                    emitSessionRecv(strong_helper, buf);
                }
                return;
            }

            //收到非本peer fd的数据，让server去派发此数据到合适的session对象
            strong_self->onRead_l(false, id, buf, addr, addr_len);
        });
        socket->setOnErr([weak_self, weak_helper, id](const SockException &err) {
            // 在本函数作用域结束时移除会话对象
            // 目的是确保移除会话前执行其 onError 函数
            // 同时避免其 onError 函数抛异常时没有移除会话对象
            onceToken token(nullptr, [&]() {
                // 移除掉会话
                auto strong_self = weak_self.lock();
                if (!strong_self) {
                    return;
                }
                // 延时移除udp session, 防止频繁快速重建对象
                strong_self->_poller->doDelayTask(kUdpDelayCloseMS, [weak_self, id]() {
                    if (auto strong_self = weak_self.lock()) {
                        // 从共享map中移除本session对象
                        lock_guard<std::recursive_mutex> lck(*strong_self->_session_mutex);
                        strong_self->_session_map->erase(id);
                    }
                    return 0;
                });
            });

            // 获取会话强应用
            if (auto strong_helper = weak_helper.lock()) {
                // 触发 onError 事件回调
                TraceP(strong_helper->session()) << strong_helper->className() << " on err: " << err;
                strong_helper->enable = false;
                strong_helper->session()->onError(err);
            }
        });

        auto pr = _session_map->emplace(id, std::move(helper));
        assert(pr.second);
        return pr.first->second;
    };

    if (socket->getPoller()->isCurrentThread()) {
        // 该socket分配在本线程，直接创建helper对象
        return helper_creator();
    }

    // 该socket分配在其他线程，需要先拷贝buffer，然后在其所在线程创建helper对象并处理数据
    auto cacheable_buf = std::make_shared<BufferString>(buf->toString());
    socket->getPoller()->async([helper_creator, cacheable_buf]() {
        // 在该socket所在线程创建helper对象
        auto helper = helper_creator();
        if (helper) {
            // 可能未实质创建hlepr对象成功，可能获取到其他线程创建的helper对象
            helper->session()->getPoller()->async([helper, cacheable_buf]() {
                // 该数据不能丢弃，给session对象消费
                emitSessionRecv(helper, cacheable_buf);
            });
        }
    });
    return nullptr;
}

void UdpServer::setOnCreateSocket(onCreateSocket cb) {
    if (cb) {
        _on_create_socket = std::move(cb);
    } else {
        _on_create_socket = [](const EventPoller::Ptr &poller, const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
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

Socket::Ptr UdpServer::createSocket(const EventPoller::Ptr &poller, const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
    return _on_create_socket(poller, buf, addr, addr_len);
}


StatisticImp(UdpServer)

} // namespace toolkit
