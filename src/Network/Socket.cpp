/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <type_traits>
#include "sockutil.h"
#include "Socket.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/uv_errno.h"
#include "Thread/semaphore.h"
#include "Poller/EventPoller.h"
#include "Thread/WorkThreadPool.h"
using namespace std;

#define LOCK_GUARD(mtx) lock_guard<decltype(mtx)> lck(mtx)

namespace toolkit {

StatisticImp(Socket)

static SockException toSockException(int error) {
    switch (error) {
        case 0:
        case UV_EAGAIN: return SockException(Err_success, "success");
        case UV_ECONNREFUSED: return SockException(Err_refused, uv_strerror(error), error);
        case UV_ETIMEDOUT: return SockException(Err_timeout, uv_strerror(error), error);
        case UV_ECONNRESET: return SockException(Err_reset, uv_strerror(error), error);
        default: return SockException(Err_other, uv_strerror(error), error);
    }
}

static SockException getSockErr(int sock, bool try_errno = true) {
    int error = 0, len = sizeof(int);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&error, (socklen_t *)&len);
    if (error == 0) {
        if (try_errno) {
            error = get_uv_error(true);
        }
    } else {
        error = uv_translate_posix_error(error);
    }
    return toSockException(error);
}

Socket::Ptr Socket::createSocket(const EventPoller::Ptr &poller_in, bool enable_mutex) {
    auto poller = poller_in ? poller_in : EventPollerPool::Instance().getPoller();
    std::weak_ptr<EventPoller> weak_poller = poller;
    return Socket::Ptr(new Socket(poller, enable_mutex), [weak_poller](Socket *ptr) {
        if (auto poller = weak_poller.lock()) {
            poller->async([ptr]() { delete ptr; });
        } else {
            delete ptr;
        }
    });
}

Socket::Socket(EventPoller::Ptr poller, bool enable_mutex)
    : _poller(std::move(poller))
    , _mtx_sock_fd(enable_mutex)
    , _mtx_event(enable_mutex)
    , _mtx_send_buf_waiting(enable_mutex)
    , _mtx_send_buf_sending(enable_mutex) {
    setOnRead(nullptr);
    setOnErr(nullptr);
    setOnAccept(nullptr);
    setOnFlush(nullptr);
    setOnBeforeAccept(nullptr);
    setOnSendResult(nullptr);
}

Socket::~Socket() {
    closeSock();
}

void Socket::setOnRead(onReadCB cb) {
    onMultiReadCB cb2;
    if (cb) {
        cb2 = [cb](Buffer::Ptr *buf, struct sockaddr_storage *addr, size_t count) {
            for (auto i = 0u; i < count; ++i) {
                cb(buf[i], (struct sockaddr *)(addr + i), sizeof(struct sockaddr_storage));
            }
        };
    }
    setOnMultiRead(std::move(cb2));
}

void Socket::setOnMultiRead(onMultiReadCB cb) {
    LOCK_GUARD(_mtx_event);
    if (cb) {
        _on_multi_read = std::move(cb);
    } else {
        _on_multi_read = [](Buffer::Ptr *buf, struct sockaddr_storage *addr, size_t count) {
            for (auto i = 0u; i < count; ++i) {
                WarnL << "Socket not set read callback, data ignored: " << buf[i]->size();
            }
        };
    }
}

void Socket::setOnErr(onErrCB cb) {
    LOCK_GUARD(_mtx_event);
    if (cb) {
        _on_err = std::move(cb);
    } else {
        _on_err = [](const SockException &err) { WarnL << "Socket not set err callback, err: " << err; };
    }
}

void Socket::setOnAccept(onAcceptCB cb) {
    LOCK_GUARD(_mtx_event);
    if (cb) {
        _on_accept = std::move(cb);
    } else {
        _on_accept = [](Socket::Ptr &sock, shared_ptr<void> &complete) { WarnL << "Socket not set accept callback, peer fd: " << sock->rawFD(); };
    }
}

void Socket::setOnFlush(onFlush cb) {
    LOCK_GUARD(_mtx_event);
    if (cb) {
        _on_flush = std::move(cb);
    } else {
        _on_flush = []() { return true; };
    }
}

void Socket::setOnBeforeAccept(onCreateSocket cb) {
    LOCK_GUARD(_mtx_event);
    if (cb) {
        _on_before_accept = std::move(cb);
    } else {
        _on_before_accept = [](const EventPoller::Ptr &poller) { return nullptr; };
    }
}

void Socket::setOnSendResult(onSendResult cb) {
    LOCK_GUARD(_mtx_event);
    _send_result = std::move(cb);
}

void Socket::connect(const string &url, uint16_t port, const onErrCB &con_cb_in, float timeout_sec, const string &local_ip, uint16_t local_port) {
    weak_ptr<Socket> weak_self = shared_from_this();
    // 因为涉及到异步回调，所以在poller线程中执行确保线程安全
    _poller->async([=] {
        if (auto strong_self = weak_self.lock()) {
            strong_self->connect_l(url, port, con_cb_in, timeout_sec, local_ip, local_port);
        }
    });
}

void Socket::connect_l(const string &url, uint16_t port, const onErrCB &con_cb_in, float timeout_sec, const string &local_ip, uint16_t local_port) {
    // 重置当前socket
    closeSock();

    weak_ptr<Socket> weak_self = shared_from_this();
    auto con_cb = [con_cb_in, weak_self](const SockException &err) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        strong_self->_async_con_cb = nullptr;
        strong_self->_con_timer = nullptr;
        if (err) {
            strong_self->setSock(nullptr);
        }
        con_cb_in(err);
    };

    auto async_con_cb = std::make_shared<function<void(const SockNum::Ptr &)>>([weak_self, con_cb](const SockNum::Ptr &sock) {
        auto strong_self = weak_self.lock();
        if (!sock || !strong_self) {
            con_cb(SockException(Err_dns, get_uv_errmsg(true)));
            return;
        }

        // 监听该socket是否可写，可写表明已经连接服务器成功
        int result = strong_self->_poller->addEvent(sock->rawFd(), EventPoller::Event_Write | EventPoller::Event_Error, [weak_self, sock, con_cb](int event) {
            if (auto strong_self = weak_self.lock()) {
                strong_self->onConnected(sock, con_cb);
            }
        });

        if (result == -1) {
            con_cb(SockException(Err_other, std::string("add event to poller failed when start connect:") + get_uv_errmsg()));
        } else {
            // 先创建SockFD对象，防止SockNum由于未执行delEvent无法析构
            strong_self->setSock(sock);
        }
    });

    // 连接超时定时器
    _con_timer = std::make_shared<Timer>(timeout_sec,[weak_self, con_cb]() {
        con_cb(SockException(Err_timeout, uv_strerror(UV_ETIMEDOUT)));
        return false;
    }, _poller);

    if (isIP(url.data())) {
        auto fd = SockUtil::connect(url.data(), port, true, local_ip.data(), local_port);
        (*async_con_cb)(fd == -1 ? nullptr : std::make_shared<SockNum>(fd, SockNum::Sock_TCP));
    } else {
        auto poller = _poller;
        weak_ptr<function<void(const SockNum::Ptr &)>> weak_task = async_con_cb;
        WorkThreadPool::Instance().getExecutor()->async([url, port, local_ip, local_port, weak_task, poller]() {
            // 阻塞式dns解析放在后台线程执行
            int fd = SockUtil::connect(url.data(), port, true, local_ip.data(), local_port);
            auto sock = fd == -1 ? nullptr : std::make_shared<SockNum>(fd, SockNum::Sock_TCP);
            poller->async([sock, weak_task]() {
                if (auto strong_task = weak_task.lock()) {
                    (*strong_task)(sock);
                }
            });
        });
        _async_con_cb = async_con_cb;
    }
}

void Socket::onConnected(const SockNum::Ptr &sock, const onErrCB &cb) {
    auto err = getSockErr(sock->rawFd(), false);
    if (err) {
        // 连接失败
        cb(err);
        return;
    }

    // 更新地址信息
    setSock(sock);
    // 先删除之前的可写事件监听
    _poller->delEvent(sock->rawFd(), [sock](bool) {});
    if (!attachEvent(sock)) {
        // 连接失败
        cb(SockException(Err_other, "add event to poller failed when connected"));
        return;
    }

    {
        LOCK_GUARD(_mtx_sock_fd);
        if (_sock_fd) {
            _sock_fd->setConnected();
        }
    }
    // 连接成功
    cb(err);
}

bool Socket::attachEvent(const SockNum::Ptr &sock) {
    weak_ptr<Socket> weak_self = shared_from_this();
    if (sock->type() == SockNum::Sock_TCP_Server) {
        // tcp服务器
        auto result = _poller->addEvent(sock->rawFd(), EventPoller::Event_Read | EventPoller::Event_Error, [weak_self, sock](int event) {
            if (auto strong_self = weak_self.lock()) {
                strong_self->onAccept(sock, event);
            }
        });
        return -1 != result;
    }

    // tcp客户端或udp
    auto read_buffer = _poller->getSharedBuffer(sock->type() == SockNum::Sock_UDP);
    auto result = _poller->addEvent(sock->rawFd(), EventPoller::Event_Read | EventPoller::Event_Error | EventPoller::Event_Write, [weak_self, sock, read_buffer](int event) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }

        if (event & EventPoller::Event_Read) {
            strong_self->onRead(sock, read_buffer);
        }
        if (event & EventPoller::Event_Write) {
            strong_self->onWriteAble(sock);
        }
        if (event & EventPoller::Event_Error) {
            strong_self->emitErr(getSockErr(sock->rawFd()));
        }
    });

    return -1 != result;
}

ssize_t Socket::onRead(const SockNum::Ptr &sock, const SocketRecvBuffer::Ptr &buffer) noexcept {
    ssize_t ret = 0, nread = 0, count = 0;

    while (_enable_recv) {
        nread = buffer->recvFromSocket(sock->rawFd(), count);
        if (nread == 0) {
            if (sock->type() == SockNum::Sock_TCP) {
                emitErr(SockException(Err_eof, "end of file"));
            } else {
                WarnL << "Recv eof on udp socket[" << sock->rawFd() << "]";
            }
            return ret;
        }

        if (nread == -1) {
            auto err = get_uv_error(true);
            if (err != UV_EAGAIN) {
                if (sock->type() == SockNum::Sock_TCP) {
                    emitErr(toSockException(err));
                } else {
                    WarnL << "Recv err on udp socket[" << sock->rawFd() << "]: " << uv_strerror(err);
                }
            }
            return ret;
        }

        ret += nread;
        if (_enable_speed) {
            // 更新接收速率
            _recv_speed += nread;
        }

        auto &buf = buffer->getBuffer(0);
        auto &addr = buffer->getAddress(0);
        try {
            // 此处捕获异常，目的是防止数据未读尽，epoll边沿触发失效的问题
            LOCK_GUARD(_mtx_event);
            _on_multi_read(&buf, &addr, count);
        } catch (std::exception &ex) {
            ErrorL << "Exception occurred when emit on_read: " << ex.what();
        }
    }
    return 0;
}

bool Socket::emitErr(const SockException &err) noexcept {
    if (_err_emit) {
        return true;
    }
    _err_emit = true;
    weak_ptr<Socket> weak_self = shared_from_this();
    _poller->async([weak_self, err]() {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        LOCK_GUARD(strong_self->_mtx_event);
        try {
            strong_self->_on_err(err);
        } catch (std::exception &ex) {
            ErrorL << "Exception occurred when emit on_err: " << ex.what();
        }
        // 延后关闭socket，只移除其io事件，防止Session对象析构时获取fd相关信息失败
        strong_self->closeSock(false);
    });
    return true;
}

ssize_t Socket::send(const char *buf, size_t size, struct sockaddr *addr, socklen_t addr_len, bool try_flush) {
    if (size <= 0) {
        size = strlen(buf);
        if (!size) {
            return 0;
        }
    }
    auto ptr = BufferRaw::create();
    ptr->assign(buf, size);
    return send(std::move(ptr), addr, addr_len, try_flush);
}

ssize_t Socket::send(string buf, struct sockaddr *addr, socklen_t addr_len, bool try_flush) {
    return send(std::make_shared<BufferString>(std::move(buf)), addr, addr_len, try_flush);
}

ssize_t Socket::send(Buffer::Ptr buf, struct sockaddr *addr, socklen_t addr_len, bool try_flush) {
    if (!addr) {
        if (!_udp_send_dst) {
            return send_l(std::move(buf), false, try_flush);
        }
        // 本次发送未指定目标地址，但是目标定制已通过bindPeerAddr指定
        addr = (struct sockaddr *)_udp_send_dst.get();
        addr_len = SockUtil::get_sock_len(addr);
    }
    return send_l(std::make_shared<BufferSock>(std::move(buf), addr, addr_len), true, try_flush);
}

ssize_t Socket::send_l(Buffer::Ptr buf, bool is_buf_sock, bool try_flush) {
    auto size = buf ? buf->size() : 0;
    if (!size) {
        return 0;
    }

    {
        LOCK_GUARD(_mtx_send_buf_waiting);
        _send_buf_waiting.emplace_back(std::move(buf), is_buf_sock);
    }

    if (try_flush) {
        if (flushAll()) {
            return -1;
        }
    }

    return size;
}

int Socket::flushAll() {
    LOCK_GUARD(_mtx_sock_fd);

    if (!_sock_fd) {
        // 如果已断开连接或者发送超时
        return -1;
    }
    if (_sendable) {
        // 该socket可写
        return flushData(_sock_fd->sockNum(), false) ? 0 : -1;
    }

    // 该socket不可写,判断发送超时
    if (_send_flush_ticker.elapsedTime() > _max_send_buffer_ms) {
        // 如果发送列队中最老的数据距今超过超时时间限制，那么就断开socket连接
        emitErr(SockException(Err_other, "socket send timeout"));
        return -1;
    }
    return 0;
}

void Socket::onFlushed() {
    bool flag;
    {
        LOCK_GUARD(_mtx_event);
        flag = _on_flush();
    }
    if (!flag) {
        setOnFlush(nullptr);
    }
}

void Socket::closeSock(bool close_fd) {
    _sendable = true;
    _enable_recv = true;
    _enable_speed = false;
    _con_timer = nullptr;
    _async_con_cb = nullptr;
    _send_flush_ticker.resetTime();

    {
        LOCK_GUARD(_mtx_send_buf_waiting);
        _send_buf_waiting.clear();
    }

    {
        LOCK_GUARD(_mtx_send_buf_sending);
        _send_buf_sending.clear();
    }

    {
        LOCK_GUARD(_mtx_sock_fd);
        if (close_fd) {
            _err_emit = false;
            _sock_fd = nullptr;
        } else if (_sock_fd) {
            _sock_fd->delEvent();
        }
    }
}

size_t Socket::getSendBufferCount() {
    size_t ret = 0;
    {
        LOCK_GUARD(_mtx_send_buf_waiting);
        ret += _send_buf_waiting.size();
    }

    {
        LOCK_GUARD(_mtx_send_buf_sending);
        _send_buf_sending.for_each([&](BufferList::Ptr &buf) { ret += buf->count(); });
    }
    return ret;
}

uint64_t Socket::elapsedTimeAfterFlushed() {
    return _send_flush_ticker.elapsedTime();
}

int Socket::getRecvSpeed() {
    _enable_speed = true;
    return _recv_speed.getSpeed();
}

int Socket::getSendSpeed() {
    _enable_speed = true;
    return _send_speed.getSpeed();
}

bool Socket::listen(uint16_t port, const string &local_ip, int backlog) {
    closeSock();
    int fd = SockUtil::listen(port, local_ip.data(), backlog);
    if (fd == -1) {
        return false;
    }
    return fromSock_l(std::make_shared<SockNum>(fd, SockNum::Sock_TCP_Server));
}

bool Socket::bindUdpSock(uint16_t port, const string &local_ip, bool enable_reuse) {
    closeSock();
    int fd = SockUtil::bindUdpSock(port, local_ip.data(), enable_reuse);
    if (fd == -1) {
        return false;
    }
    return fromSock_l(std::make_shared<SockNum>(fd, SockNum::Sock_UDP));
}

bool Socket::fromSock(int fd, SockNum::SockType type) {
    closeSock();
    SockUtil::setNoSigpipe(fd);
    SockUtil::setNoBlocked(fd);
    SockUtil::setCloExec(fd);
    return fromSock_l(std::make_shared<SockNum>(fd, type));
}

bool Socket::fromSock_l(SockNum::Ptr sock) {
    if (!attachEvent(sock)) {
        return false;
    }
    setSock(std::move(sock));
    return true;
}

int Socket::onAccept(const SockNum::Ptr &sock, int event) noexcept {
    int fd;
    struct sockaddr_storage peer_addr;
    socklen_t addr_len = sizeof(peer_addr);
    while (true) {
        if (event & EventPoller::Event_Read) {
            do {
                fd = (int)accept(sock->rawFd(), (struct sockaddr *)&peer_addr, &addr_len);
            } while (-1 == fd && UV_EINTR == get_uv_error(true));

            if (fd == -1) {
                // accept失败
                int err = get_uv_error(true);
                if (err == UV_EAGAIN) {
                    // 没有新连接
                    return 0;
                }
                auto ex = toSockException(err);
                // emitErr(ex); https://github.com/ZLMediaKit/ZLMediaKit/issues/2946
                ErrorL << "Accept socket failed: " << ex.what();
                // 可能打开的文件描述符太多了:UV_EMFILE/UV_ENFILE
#if defined(HAS_EPOLL) &&  !defined(_WIN32) 
                // 边缘触发，还需要手动再触发accept事件,  
                //wepoll, Edge-triggered (`EPOLLET`) mode isn't supported.
                std::weak_ptr<Socket> weak_self = shared_from_this();
                _poller->doDelayTask(100, [weak_self, sock]() {
                    if (auto strong_self = weak_self.lock()) {
                        // 100ms后再处理accept事件，说不定已经有空闲的fd
                        strong_self->onAccept(sock, EventPoller::Event_Read);
                    }
                    return 0;
                });
                // 暂时不处理accept事件，等待100ms后手动触发onAccept(只有EAGAIN读空后才能通过epoll再次触发事件)
                return -1;
#else
                // 水平触发；休眠10ms，防止无谓的accept失败
                this_thread::sleep_for(std::chrono::milliseconds(10));
                // 暂时不处理accept事件，由于是水平触发，下次还会再次自动进入onAccept函数
                return -1;
#endif
            }

            SockUtil::setNoSigpipe(fd);
            SockUtil::setNoBlocked(fd);
            SockUtil::setNoDelay(fd);
            SockUtil::setSendBuf(fd);
            SockUtil::setRecvBuf(fd);
            SockUtil::setCloseWait(fd);
            SockUtil::setCloExec(fd);

            Socket::Ptr peer_sock;
            try {
                // 此处捕获异常，目的是防止socket未accept尽，epoll边沿触发失效的问题
                LOCK_GUARD(_mtx_event);
                // 拦截Socket对象的构造
                peer_sock = _on_before_accept(_poller);
            } catch (std::exception &ex) {
                ErrorL << "Exception occurred when emit on_before_accept: " << ex.what();
                close(fd);
                continue;
            }

            if (!peer_sock) {
                // 此处是默认构造行为，也就是子Socket共用父Socket的poll线程并且关闭互斥锁
                peer_sock = Socket::createSocket(_poller, false);
            }

            auto sock = std::make_shared<SockNum>(fd, SockNum::Sock_TCP);
            // 设置好fd,以备在onAccept事件中可以正常访问该fd
            peer_sock->setSock(sock);
            // 赋值peer ip，防止在执行setSock时，fd已经被reset断开
            memcpy(&peer_sock->_peer_addr, &peer_addr, addr_len);

            shared_ptr<void> completed(nullptr, [peer_sock, sock](void *) {
                try {
                    // 然后把该fd加入poll监听(确保先触发onAccept事件然后再触发onRead等事件)
                    if (!peer_sock->attachEvent(sock)) {
                        // 加入poll监听失败，触发onErr事件，通知该Socket无效
                        peer_sock->emitErr(SockException(Err_eof, "add event to poller failed when accept a socket"));
                    }
                } catch (std::exception &ex) {
                    ErrorL << "Exception occurred: " << ex.what();
                }
            });

            try {
                // 此处捕获异常，目的是防止socket未accept尽，epoll边沿触发失效的问题
                LOCK_GUARD(_mtx_event);
                // 先触发onAccept事件，此时应该监听该Socket的onRead等事件
                _on_accept(peer_sock, completed);
            } catch (std::exception &ex) {
                ErrorL << "Exception occurred when emit on_accept: " << ex.what();
                continue;
            }
        }

        if (event & EventPoller::Event_Error) {
            auto ex = getSockErr(sock->rawFd());
            emitErr(ex);
            ErrorL << "TCP listener occurred a err: " << ex.what();
            return -1;
        }
    }
}

void Socket::setSock(SockNum::Ptr sock) {
    LOCK_GUARD(_mtx_sock_fd);
    if (sock) {
        _sock_fd = std::make_shared<SockFD>(std::move(sock), _poller);
        SockUtil::get_sock_local_addr(_sock_fd->rawFd(), _local_addr);
        SockUtil::get_sock_peer_addr(_sock_fd->rawFd(), _peer_addr);
    } else {
        _sock_fd = nullptr;
    }
}

string Socket::get_local_ip() {
    LOCK_GUARD(_mtx_sock_fd);
    if (!_sock_fd) {
        return "";
    }
    return SockUtil::inet_ntoa((struct sockaddr *)&_local_addr);
}

uint16_t Socket::get_local_port() {
    LOCK_GUARD(_mtx_sock_fd);
    if (!_sock_fd) {
        return 0;
    }
    return SockUtil::inet_port((struct sockaddr *)&_local_addr);
}

string Socket::get_peer_ip() {
    LOCK_GUARD(_mtx_sock_fd);
    if (!_sock_fd) {
        return "";
    }
    if (_udp_send_dst) {
        return SockUtil::inet_ntoa((struct sockaddr *)_udp_send_dst.get());
    }
    return SockUtil::inet_ntoa((struct sockaddr *)&_peer_addr);
}

uint16_t Socket::get_peer_port() {
    LOCK_GUARD(_mtx_sock_fd);
    if (!_sock_fd) {
        return 0;
    }
    if (_udp_send_dst) {
        return SockUtil::inet_port((struct sockaddr *)_udp_send_dst.get());
    }
    return SockUtil::inet_port((struct sockaddr *)&_peer_addr);
}

string Socket::getIdentifier() const {
    static string class_name = "Socket: ";
    return class_name + to_string(reinterpret_cast<uint64_t>(this));
}

bool Socket::flushData(const SockNum::Ptr &sock, bool poller_thread) {
    decltype(_send_buf_sending) send_buf_sending_tmp;
    {
        // 转移出二级缓存
        LOCK_GUARD(_mtx_send_buf_sending);
        if (!_send_buf_sending.empty()) {
            send_buf_sending_tmp.swap(_send_buf_sending);
        }
    }

    if (send_buf_sending_tmp.empty()) {
        _send_flush_ticker.resetTime();
        do {
            {
                // 二级发送缓存为空，那么我们接着消费一级缓存中的数据
                LOCK_GUARD(_mtx_send_buf_waiting);
                if (!_send_buf_waiting.empty()) {
                    // 把一级缓中数数据放置到二级缓存中并清空
                    LOCK_GUARD(_mtx_event);
                    auto send_result = _enable_speed ? [this](const Buffer::Ptr &buffer, bool send_success) {
                        if (send_success) {
                            //更新发送速率
                            _send_speed += buffer->size();
                        }
                        LOCK_GUARD(_mtx_event);
                        if (_send_result) {
                            _send_result(buffer, send_success);
                        }
                    } : _send_result;
                    send_buf_sending_tmp.emplace_back(BufferList::create(std::move(_send_buf_waiting), std::move(send_result), sock->type() == SockNum::Sock_UDP));
                    break;
                }
            }
            // 如果一级缓存也为空,那么说明所有数据均写入socket了
            if (poller_thread) {
                // poller线程触发该函数，那么该socket应该已经加入了可写事件的监听；
                // 那么在数据列队清空的情况下，我们需要关闭监听以免触发无意义的事件回调
                stopWriteAbleEvent(sock);
                onFlushed();
            }
            return true;
        } while (false);
    }

    while (!send_buf_sending_tmp.empty()) {
        auto &packet = send_buf_sending_tmp.front();
        auto n = packet->send(sock->rawFd(), _sock_flags);
        if (n > 0) {
            // 全部或部分发送成功
            if (packet->empty()) {
                // 全部发送成功
                send_buf_sending_tmp.pop_front();
                continue;
            }
            // 部分发送成功
            if (!poller_thread) {
                // 如果该函数是poller线程触发的，那么该socket应该已经加入了可写事件的监听，所以我们不需要再次加入监听
                startWriteAbleEvent(sock);
            }
            break;
        }

        // 一个都没发送成功
        int err = get_uv_error(true);
        if (err == UV_EAGAIN) {
            // 等待下一次发送
            if (!poller_thread) {
                // 如果该函数是poller线程触发的，那么该socket应该已经加入了可写事件的监听，所以我们不需要再次加入监听
                startWriteAbleEvent(sock);
            }
            break;
        }

        // 其他错误代码，发生异常
        if (sock->type() == SockNum::Sock_UDP) {
            // udp发送异常，把数据丢弃
            send_buf_sending_tmp.pop_front();
            WarnL << "Send udp socket[" << sock << "] failed, data ignored: " << uv_strerror(err);
            continue;
        }
        // tcp发送失败时，触发异常
        emitErr(toSockException(err));
        return false;
    }

    // 回滚未发送完毕的数据
    if (!send_buf_sending_tmp.empty()) {
        // 有剩余数据
        LOCK_GUARD(_mtx_send_buf_sending);
        send_buf_sending_tmp.swap(_send_buf_sending);
        _send_buf_sending.append(send_buf_sending_tmp);
        // 二级缓存未全部发送完毕，说明该socket不可写，直接返回
        return true;
    }

    // 二级缓存已经全部发送完毕，说明该socket还可写，我们尝试继续写
    // 如果是poller线程，我们尝试再次写一次(因为可能其他线程调用了send函数又有新数据了)
    return poller_thread ? flushData(sock, poller_thread) : true;
}

void Socket::onWriteAble(const SockNum::Ptr &sock) {
    bool empty_waiting;
    bool empty_sending;
    {
        LOCK_GUARD(_mtx_send_buf_waiting);
        empty_waiting = _send_buf_waiting.empty();
    }

    {
        LOCK_GUARD(_mtx_send_buf_sending);
        empty_sending = _send_buf_sending.empty();
    }

    if (empty_waiting && empty_sending) {
        // 数据已经清空了，我们停止监听可写事件
        stopWriteAbleEvent(sock);
    } else {
        // socket可写，我们尝试发送剩余的数据
        flushData(sock, true);
    }
}

void Socket::startWriteAbleEvent(const SockNum::Ptr &sock) {
    // 开始监听socket可写事件
    _sendable = false;
    int flag = _enable_recv ? EventPoller::Event_Read : 0;
    _poller->modifyEvent(sock->rawFd(), flag | EventPoller::Event_Error | EventPoller::Event_Write, [sock](bool) {});
}

void Socket::stopWriteAbleEvent(const SockNum::Ptr &sock) {
    // 停止监听socket可写事件
    _sendable = true;
    int flag = _enable_recv ? EventPoller::Event_Read : 0;
    _poller->modifyEvent(sock->rawFd(), flag | EventPoller::Event_Error, [sock](bool) {});
}

void Socket::enableRecv(bool enabled) {
    if (_enable_recv == enabled) {
        return;
    }
    _enable_recv = enabled;
    int read_flag = _enable_recv ? EventPoller::Event_Read : 0;
    // 可写时，不监听可写事件
    int send_flag = _sendable ? 0 : EventPoller::Event_Write;
    _poller->modifyEvent(rawFD(), read_flag | send_flag | EventPoller::Event_Error);
}

int Socket::rawFD() const {
    LOCK_GUARD(_mtx_sock_fd);
    if (!_sock_fd) {
        return -1;
    }
    return _sock_fd->rawFd();
}

bool Socket::alive() const {
    LOCK_GUARD(_mtx_sock_fd);
    return _sock_fd && !_err_emit;
}

SockNum::SockType Socket::sockType() const {
    LOCK_GUARD(_mtx_sock_fd);
    if (!_sock_fd) {
        return SockNum::Sock_Invalid;
    }
    return _sock_fd->type();
}

void Socket::setSendTimeOutSecond(uint32_t second) {
    _max_send_buffer_ms = second * 1000;
}

bool Socket::isSocketBusy() const {
    return !_sendable.load();
}

const EventPoller::Ptr &Socket::getPoller() const {
    return _poller;
}

bool Socket::cloneSocket(const Socket &other) {
    closeSock();
    SockNum::Ptr sock;
    {
        LOCK_GUARD(other._mtx_sock_fd);
        if (!other._sock_fd) {
            WarnL << "sockfd of src socket is null";
            return false;
        }
        sock = other._sock_fd->sockNum();
    }
    return fromSock_l(sock);
}

bool Socket::bindPeerAddr(const struct sockaddr *dst_addr, socklen_t addr_len, bool soft_bind) {
    LOCK_GUARD(_mtx_sock_fd);
    if (!_sock_fd) {
        return false;
    }
    if (_sock_fd->type() != SockNum::Sock_UDP) {
        return false;
    }
    addr_len = addr_len ? addr_len : SockUtil::get_sock_len(dst_addr);
    if (soft_bind) {
        // 软绑定，只保存地址
        _udp_send_dst = std::make_shared<struct sockaddr_storage>();
        memcpy(_udp_send_dst.get(), dst_addr, addr_len);
    } else {
        // 硬绑定后，取消软绑定，防止memcpy目标地址的性能损失
        _udp_send_dst = nullptr;
        if (-1 == ::connect(_sock_fd->rawFd(), dst_addr, addr_len)) {
            WarnL << "Connect socket to peer address failed: " << SockUtil::inet_ntoa(dst_addr);
            return false;
        }
        memcpy(&_peer_addr, dst_addr, addr_len);
    }
    return true;
}

void Socket::setSendFlags(int flags) {
    _sock_flags = flags;
}

///////////////SockSender///////////////////

SockSender &SockSender::operator<<(const char *buf) {
    send(buf);
    return *this;
}

SockSender &SockSender::operator<<(string buf) {
    send(std::move(buf));
    return *this;
}

SockSender &SockSender::operator<<(Buffer::Ptr buf) {
    send(std::move(buf));
    return *this;
}

ssize_t SockSender::send(string buf) {
    return send(std::make_shared<BufferString>(std::move(buf)));
}

ssize_t SockSender::send(const char *buf, size_t size) {
    auto buffer = BufferRaw::create();
    buffer->assign(buf, size);
    return send(std::move(buffer));
}

///////////////SocketHelper///////////////////

SocketHelper::SocketHelper(const Socket::Ptr &sock) {
    setSock(sock);
    setOnCreateSocket(nullptr);
}

void SocketHelper::setPoller(const EventPoller::Ptr &poller) {
    _poller = poller;
}

void SocketHelper::setSock(const Socket::Ptr &sock) {
    _sock = sock;
    if (_sock) {
        _poller = _sock->getPoller();
    }
}

const EventPoller::Ptr &SocketHelper::getPoller() const {
    assert(_poller);
    return _poller;
}

const Socket::Ptr &SocketHelper::getSock() const {
    return _sock;
}

int SocketHelper::flushAll() {
    if (!_sock) {
        return -1;
    }
    return _sock->flushAll();
}

ssize_t SocketHelper::send(Buffer::Ptr buf) {
    if (!_sock) {
        return -1;
    }
    return _sock->send(std::move(buf), nullptr, 0, _try_flush);
}

void SocketHelper::shutdown(const SockException &ex) {
    if (_sock) {
        _sock->emitErr(ex);
    }
}

void SocketHelper::safeShutdown(const SockException &ex) {
    std::weak_ptr<SocketHelper> weak_self = shared_from_this();
    async_first([weak_self, ex]() {
        if (auto strong_self = weak_self.lock()) {
            strong_self->shutdown(ex);
        }
    });
}

string SocketHelper::get_local_ip() {
    return _sock ? _sock->get_local_ip() : "";
}

uint16_t SocketHelper::get_local_port() {
    return _sock ? _sock->get_local_port() : 0;
}

string SocketHelper::get_peer_ip() {
    return _sock ? _sock->get_peer_ip() : "";
}

uint16_t SocketHelper::get_peer_port() {
    return _sock ? _sock->get_peer_port() : 0;
}

bool SocketHelper::isSocketBusy() const {
    if (!_sock) {
        return true;
    }
    return _sock->isSocketBusy();
}

Task::Ptr SocketHelper::async(TaskIn task, bool may_sync) {
    return _poller->async(std::move(task), may_sync);
}

Task::Ptr SocketHelper::async_first(TaskIn task, bool may_sync) {
    return _poller->async_first(std::move(task), may_sync);
}

void SocketHelper::setSendFlushFlag(bool try_flush) {
    _try_flush = try_flush;
}

void SocketHelper::setSendFlags(int flags) {
    if (!_sock) {
        return;
    }
    _sock->setSendFlags(flags);
}

void SocketHelper::setOnCreateSocket(Socket::onCreateSocket cb) {
    if (cb) {
        _on_create_socket = std::move(cb);
    } else {
        _on_create_socket = [](const EventPoller::Ptr &poller) { return Socket::createSocket(poller, false); };
    }
}

Socket::Ptr SocketHelper::createSocket() {
    return _on_create_socket(_poller);
}

std::ostream &operator<<(std::ostream &ost, const SockException &err) {
    ost << err.getErrCode() << "(" << err.what() << ")";
    return ost;
}

} // namespace toolkit
