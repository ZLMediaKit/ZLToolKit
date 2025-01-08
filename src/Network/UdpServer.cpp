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
            ret[0] = ((struct sockaddr_in *) addr)->sin_port >> 8;
            ret[1] = ((struct sockaddr_in *) addr)->sin_port & 0xFF;
            //ipv4地址统一转换为ipv6方式处理  [AUTO-TRANSLATED:ad7cf8c3]
            //Convert ipv4 addresses to ipv6 for unified processing
            memcpy(&ret[2], &s_in6_addr_maped, 12);
            memcpy(&ret[14], &(((struct sockaddr_in *) addr)->sin_addr), 4);
            return ret;
        }
        case AF_INET6 : {
            ret[0] = ((struct sockaddr_in6 *) addr)->sin6_port >> 8;
            ret[1] = ((struct sockaddr_in6 *) addr)->sin6_port & 0xFF;
            memcpy(&ret[2], &(((struct sockaddr_in6 *)addr)->sin6_addr), 16);
            return ret;
        }
        default: throw std::invalid_argument("invalid sockaddr address");
    }
}

UdpServer::UdpServer(const EventPoller::Ptr &poller) : Server(poller) {
    _multi_poller = !poller;
    setOnCreateSocket(nullptr);
}

void UdpServer::setupEvent() {
    _socket = createSocket(_poller);
    std::weak_ptr<UdpServer> weak_self = std::static_pointer_cast<UdpServer>(shared_from_this());
    _socket->setOnRead([weak_self](Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
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
    //主server才创建session map，其他cloned server共享之  [AUTO-TRANSLATED:113cf4fd]
    //Only the main server creates a session map, other cloned servers share it
    _session_mutex = std::make_shared<std::recursive_mutex>();
    _session_map = std::make_shared<SessionMapType>();

    // 新建一个定时器定时管理这些 udp 会话,这些对象只由主server做超时管理，cloned server不管理  [AUTO-TRANSLATED:d20478a2]
    //Create a timer to manage these udp sessions periodically, these objects are only managed by the main server, cloned servers do not manage them
    std::weak_ptr<UdpServer> weak_self = std::static_pointer_cast<UdpServer>(shared_from_this());
    _timer = std::make_shared<Timer>(2.0f, [weak_self]() -> bool {
        if (auto strong_self = weak_self.lock()) {
            strong_self->onManagerSession();
            return true;
        }
        return false;
    }, _poller);

    if (_multi_poller) {
        // clone server至不同线程，让udp server支持多线程  [AUTO-TRANSLATED:15a85c8f]
        //Clone the server to different threads to support multi-threading for the udp server
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
    }

    if (!_socket->bindUdpSock(port, host.c_str())) {
        // udp 绑定端口失败, 可能是由于端口占用或权限问题  [AUTO-TRANSLATED:c31eedba]
        //Failed to bind udp port, possibly due to port occupation or permission issues
        std::string err = (StrPrinter << "Bind udp socket on " << host << " " << port << " failed: " << get_uv_errmsg(true));
        throw std::runtime_error(err);
    }

    for (auto &pr: _cloned_server) {
        // 启动子Server  [AUTO-TRANSLATED:1820131c]
        //Start the child server
#if 0
        pr.second->_socket->cloneSocket(*_socket);
#else
        // 实验发现cloneSocket方式虽然可以节省fd资源，但是在某些系统上线程漂移问题更严重  [AUTO-TRANSLATED:d6a88e17]
        //Experiments have found that the cloneSocket method can save fd resources, but the thread drift problem is more serious on some systems
        pr.second->_socket->bindUdpSock(_socket->get_local_port(), _socket->get_local_ip());
#endif
    }
    InfoL << "UDP server bind to [" << host << "]: " << port;
}

UdpServer::Ptr UdpServer::onCreatServer(const EventPoller::Ptr &poller) {
    return Ptr(new UdpServer(poller), [poller](UdpServer *ptr) { poller->async([ptr]() { delete ptr; }); });
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

void UdpServer::onRead(Buffer::Ptr &buf, sockaddr *addr, int addr_len) {
    const auto id = makeSockId(addr, addr_len);
    onRead_l(true, id, buf, addr, addr_len);
}

static void emitSessionRecv(const SessionHelper::Ptr &helper, const Buffer::Ptr &buf) {
    if (!helper->enable) {
        // 延时销毁中  [AUTO-TRANSLATED:24d3d333]
        //Delayed destruction in progress
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

void UdpServer::onRead_l(bool is_server_fd, const UdpServer::PeerIdType &id, Buffer::Ptr &buf, sockaddr *addr, int addr_len) {
    // udp server fd收到数据时触发此函数；大部分情况下数据应该在peer fd触发，此函数应该不是热点函数  [AUTO-TRANSLATED:f347ff20]
    //This function is triggered when the udp server fd receives data; in most cases, the data should be triggered by the peer fd, and this function should not be a hot spot
    bool is_new = false;
    if (auto helper = getOrCreateSession(id, buf, addr, addr_len, is_new)) {
        if (helper->session()->getPoller()->isCurrentThread()) {
            //当前线程收到数据，直接处理数据  [AUTO-TRANSLATED:07e5a596]
            //The current thread receives data and processes it directly
            emitSessionRecv(helper, buf);
        } else {
            //数据漂移到其他线程，需要先切换线程  [AUTO-TRANSLATED:15235f6f]
            //Data migration to another thread requires switching threads first
            WarnL << "UDP packet incoming from other thread";
            std::weak_ptr<SessionHelper> weak_helper = helper;
            //由于socket读buffer是该线程上所有socket共享复用的，所以不能跨线程使用，必须先转移走  [AUTO-TRANSLATED:1134538b]
            //Since the socket read buffer is shared and reused by all sockets on this thread, it cannot be used across threads and must be transferred first
            auto cacheable_buf = std::move(buf);
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
        //拷贝map，防止遍历时移除对象  [AUTO-TRANSLATED:ebbc7595]
        //Copy the map to prevent objects from being removed during traversal
        copy_map = std::make_shared<SessionMapType>(*_session_map);
    }
    auto lam = [copy_map]() {
        for (auto &pr : *copy_map) {
            auto &session = pr.second->session();
            if (!session->getPoller()->isCurrentThread()) {
                // 该session不归属该poller管理  [AUTO-TRANSLATED:d5edb552]
                //This session does not belong to the management of this poller
                continue;
            }
            try {
                // UDP 会话需要处理超时  [AUTO-TRANSLATED:0a51f8a1]
                //UDP sessions need to handle timeouts
                session->onManager();
            } catch (exception &ex) {
                WarnL << "Exception occurred when emit onManager: " << ex.what();
            }
        }
    };
    if (_multi_poller){
        EventPollerPool::Instance().for_each([lam](const TaskExecutor::Ptr &executor) {
            std::static_pointer_cast<EventPoller>(executor)->async(lam);
        });
    } else {
        lam();
    }
}

SessionHelper::Ptr UdpServer::getOrCreateSession(const UdpServer::PeerIdType &id, Buffer::Ptr &buf, sockaddr *addr, int addr_len, bool &is_new) {
    {
        //减小临界区  [AUTO-TRANSLATED:3d6089d8]
        //Reduce the critical section
        std::lock_guard<std::recursive_mutex> lock(*_session_mutex);
        auto it = _session_map->find(id);
        if (it != _session_map->end()) {
            return it->second;
        }
    }
    is_new = true;
    return createSession(id, buf, addr, addr_len);
}

SessionHelper::Ptr UdpServer::createSession(const PeerIdType &id, Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
    // 此处改成自定义获取poller对象，防止负载不均衡  [AUTO-TRANSLATED:194e8460]
    //Change to custom acquisition of poller objects to prevent load imbalance
    auto socket = createSocket(_multi_poller ? EventPollerPool::Instance().getPoller(false) : _poller, buf, addr, addr_len);
    if (!socket) {
        //创建socket失败，本次onRead事件收到的数据直接丢弃  [AUTO-TRANSLATED:b218d68c]
        //Socket creation failed, the data received by this onRead event is discarded
        return nullptr;
    }

    auto addr_str = string((char *) addr, addr_len);
    std::weak_ptr<UdpServer> weak_self = std::static_pointer_cast<UdpServer>(shared_from_this());
    auto helper_creator = [this, weak_self, socket, addr_str, id]() -> SessionHelper::Ptr {
        auto server = weak_self.lock();
        if (!server) {
            return nullptr;
        }

        //如果已经创建该客户端对应的UdpSession类，那么直接返回  [AUTO-TRANSLATED:c57a0d71]
        //If the UdpSession class corresponding to this client has already been created, return directly
        lock_guard<std::recursive_mutex> lck(*_session_mutex);
        auto it = _session_map->find(id);
        if (it != _session_map->end()) {
            return it->second;
        }

        assert(_socket);
        socket->bindUdpSock(_socket->get_local_port(), _socket->get_local_ip());
        socket->bindPeerAddr((struct sockaddr *) addr_str.data(), addr_str.size());

        auto helper = _session_alloc(server, socket);
        // 把本服务器的配置传递给 Session  [AUTO-TRANSLATED:e3ed95ab]
        //Pass the configuration of this server to the Session
        helper->session()->attachServer(*this);

        std::weak_ptr<SessionHelper> weak_helper = helper;
        socket->setOnRead([weak_self, weak_helper, id](Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }
            auto new_id = makeSockId(addr, addr_len);
            //快速判断是否为本会话的的数据, 通常应该成立  [AUTO-TRANSLATED:d5d147e4]
            //Quickly determine if it's data for the current session, usually should be true
            if (id == new_id) {
                if (auto strong_helper = weak_helper.lock()) {
                    emitSessionRecv(strong_helper, buf);
                }
                return;
            }

            //收到非本peer fd的数据，让server去派发此数据到合适的session对象  [AUTO-TRANSLATED:e5f44445]
            //Received data from a non-current peer fd, let the server dispatch this data to the appropriate session object
            strong_self->onRead_l(false, new_id, buf, addr, addr_len);
        });
        socket->setOnErr([weak_self, weak_helper, id](const SockException &err) {
            // 在本函数作用域结束时移除会话对象  [AUTO-TRANSLATED:b2ade305]
            //Remove the session object when this function scope ends
            // 目的是确保移除会话前执行其 onError 函数  [AUTO-TRANSLATED:7d0329d7]
            //The purpose is to ensure the onError function is executed before removing the session
            // 同时避免其 onError 函数抛异常时没有移除会话对象  [AUTO-TRANSLATED:354191bd]
            //And avoid not removing the session object when its onError function throws an exception
            onceToken token(nullptr, [&]() {
                // 移除掉会话  [AUTO-TRANSLATED:1d786335]
                //Remove the session
                auto strong_self = weak_self.lock();
                if (!strong_self) {
                    return;
                }
                // 延时移除udp session, 防止频繁快速重建对象  [AUTO-TRANSLATED:50dbd694]
                //Delay removing the UDP session to prevent frequent and rapid object reconstruction
                strong_self->_poller->doDelayTask(kUdpDelayCloseMS, [weak_self, id]() {
                    if (auto strong_self = weak_self.lock()) {
                        // 从共享map中移除本session对象  [AUTO-TRANSLATED:47ecbf11]
                        //Remove the current session object from the shared map
                        lock_guard<std::recursive_mutex> lck(*strong_self->_session_mutex);
                        strong_self->_session_map->erase(id);
                    }
                    return 0;
                });
            });

            // 获取会话强应用  [AUTO-TRANSLATED:42283ea0]
            //Get a strong reference to the session
            if (auto strong_helper = weak_helper.lock()) {
                // 触发 onError 事件回调  [AUTO-TRANSLATED:82070c3c]
                //Trigger the onError event callback
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
        // 该socket分配在本线程，直接创建helper对象  [AUTO-TRANSLATED:18c9d95b]
        //This socket is allocated in this thread, directly create a helper object
        return helper_creator();
    }

    // 该socket分配在其他线程，需要先转移走buffer，然后在其所在线程创建helper对象并处理数据  [AUTO-TRANSLATED:7816a13f]
    //This socket is allocated in another thread, need to transfer the buffer first, then create a helper object in its thread and process the data
    auto cacheable_buf = std::move(buf);
    socket->getPoller()->async([helper_creator, cacheable_buf]() {
        // 在该socket所在线程创建helper对象  [AUTO-TRANSLATED:db8d6622]
        //Create a helper object in the thread where the socket is located
        auto helper = helper_creator();
        if (helper) {
            // 可能未实质创建hlepr对象成功，可能获取到其他线程创建的helper对象  [AUTO-TRANSLATED:091f648e]
            //May not have actually created a helper object successfully, may have obtained a helper object created by another thread
            helper->session()->getPoller()->async([helper, cacheable_buf]() {
                // 该数据不能丢弃，给session对象消费  [AUTO-TRANSLATED:6941e5fa]
                //This data cannot be discarded, provided to the session object for consumption
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
