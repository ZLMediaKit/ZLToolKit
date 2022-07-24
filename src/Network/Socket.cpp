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
        default: return SockException(Err_other, uv_strerror(error), error);
    }
}

static SockException getSockErr(const SockFD::Ptr &sock, bool try_errno = true) {
    int error = 0, len = sizeof(int);
    getsockopt(sock->rawFd(), SOL_SOCKET, SO_ERROR, (char *) &error, (socklen_t *) &len);
    if (error == 0) {
        if (try_errno) {
            error = get_uv_error(true);
        }
    } else {
        error = uv_translate_posix_error(error);
    }
    return toSockException(error);
}

Socket::Ptr Socket::createSocket(const EventPoller::Ptr &poller, bool enable_mutex){
    return std::make_shared<Socket>(poller, enable_mutex);
}

Socket::Socket(const EventPoller::Ptr &poller, bool enable_mutex) :
        _mtx_sock_fd(enable_mutex), _mtx_event(enable_mutex),
        _mtx_send_buf_waiting(enable_mutex), _mtx_send_buf_sending(enable_mutex){

    _poller = poller;
    if (!_poller) {
        _poller = EventPollerPool::Instance().getPoller();
    }
    setOnRead(nullptr);
    setOnErr(nullptr);
    setOnAccept(nullptr);
    setOnFlush(nullptr);
    setOnBeforeAccept(nullptr);
}

Socket::~Socket() {
    closeSock();
}

void Socket::setOnRead(onReadCB cb) {
    LOCK_GUARD(_mtx_event);
    if (cb) {
        _on_read = std::move(cb);
    } else {
        _on_read = [](const Buffer::Ptr &buf, struct sockaddr *, int) {
            WarnL << "Socket not set read callback, data ignored:" << buf->size();
        };
    }
}

void Socket::setOnErr(onErrCB cb) {
    LOCK_GUARD(_mtx_event);
    if (cb) {
        _on_err = std::move(cb);
    } else {
        _on_err = [](const SockException &err) {
            WarnL << "Socket not set err callback, err:" << err.what();
        };
    }
}

void Socket::setOnAccept(onAcceptCB cb) {
    LOCK_GUARD(_mtx_event);
    if (cb) {
        _on_accept = std::move(cb);
    } else {
        _on_accept = [](Socket::Ptr &sock, shared_ptr<void> &complete) {
            WarnL << "Socket not set accept callback, peer fd:" << sock->rawFD();
        };
    }
}

void Socket::setOnFlush(onFlush cb) {
    LOCK_GUARD(_mtx_event);
    if (cb) {
        _on_flush = std::move(cb);
    } else {
        _on_flush = []() {return true;};
    }
}

void Socket::setOnBeforeAccept(onCreateSocket cb){
    LOCK_GUARD(_mtx_event);
    if (cb) {
        _on_before_accept = std::move(cb);
    } else {
        _on_before_accept = [](const EventPoller::Ptr &poller) {
            return nullptr;
        };
    }
}

void Socket::setOnSendResult(onSendResult cb) {
    LOCK_GUARD(_mtx_event);
    _send_result = std::move(cb);
}

#define CLOSE_SOCK(fd) if(fd != -1) {close(fd);}

void Socket::connect(const string &url, uint16_t port, const onErrCB &con_cb_in, float timeout_sec, const string &local_ip, uint16_t local_port) {
    weak_ptr<Socket> weak_self = shared_from_this();
    _poller->async([=] {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        strong_self->connect_l(url, port, con_cb_in, timeout_sec, local_ip, local_port);
     });
}

void Socket::connect_l(const string &url, uint16_t port, const onErrCB &con_cb_in, float timeout_sec, const string &local_ip, uint16_t local_port) {
    //重置当前socket
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
            LOCK_GUARD(strong_self->_mtx_sock_fd);
            strong_self->_sock_fd = nullptr;
        }
        con_cb_in(err);
    };

    auto async_con_cb = std::make_shared<function<void(int)> >([weak_self, con_cb](int sock) {
        auto strong_self = weak_self.lock();
        if (sock == -1 || !strong_self) {
            if (!strong_self) {
                CLOSE_SOCK(sock);
            } else {
                con_cb(SockException(Err_dns, get_uv_errmsg(true)));
            }
            return;
        }

        auto sock_fd = strong_self->makeSock(sock, SockNum::Sock_TCP);
        weak_ptr<SockFD> weak_sock_fd = sock_fd;

        //监听该socket是否可写，可写表明已经连接服务器成功
        int result = strong_self->_poller->addEvent(sock, EventPoller::Event_Write, [weak_self, weak_sock_fd, con_cb](int event) {
            auto strong_sock_fd = weak_sock_fd.lock();
            auto strong_self = weak_self.lock();
            if (strong_sock_fd && strong_self) {
                //socket可写事件，说明已经连接服务器成功
                strong_self->onConnected(strong_sock_fd, con_cb);
            }
        });

        if (result == -1) {
            con_cb(SockException(Err_other, "add event to poller failed when start connect"));
            return;
        }

        //保存fd
        LOCK_GUARD(strong_self->_mtx_sock_fd);
        strong_self->_sock_fd = sock_fd;
    });

    if (isIP(url.data())) {
        (*async_con_cb)(SockUtil::connect(url.data(), port, true, local_ip.data(), local_port));
    } else {
        auto poller = _poller;
        weak_ptr<function<void(int)>> weak_task = async_con_cb;
        WorkThreadPool::Instance().getExecutor()->async([url, port, local_ip, local_port, weak_task, poller]() {
            //阻塞式dns解析放在后台线程执行
            int sock = SockUtil::connect(url.data(), port, true, local_ip.data(), local_port);
            poller->async([sock, weak_task]() {
                auto strong_task = weak_task.lock();
                if (strong_task) {
                    (*strong_task)(sock);
                } else {
                    CLOSE_SOCK(sock);
                }
            });
        });
        _async_con_cb = async_con_cb;
    }

    //连接超时定时器
    _con_timer = std::make_shared<Timer>(timeout_sec, [weak_self, con_cb]() {
        con_cb(SockException(Err_timeout, uv_strerror(UV_ETIMEDOUT)));
        return false;
    }, _poller);
}

void Socket::onConnected(const SockFD::Ptr &sock, const onErrCB &cb) {
    auto err = getSockErr(sock, false);
    if (err) {
        //连接失败
        cb(err);
        return;
    }

    //先删除之前的可写事件监听
    _poller->delEvent(sock->rawFd());
    if (!attachEvent(sock, false)) {
        //连接失败
        cb(SockException(Err_other, "add event to poller failed when connected"));
        return;
    }

    sock->setConnected();
    //连接成功
    cb(err);
}

bool Socket::attachEvent(const SockFD::Ptr &sock, bool is_udp) {
    weak_ptr<Socket> weak_self = shared_from_this();
    weak_ptr<SockFD> weak_sock = sock;
    _enable_recv = true;
    _read_buffer = _poller->getSharedBuffer();
    int result = _poller->addEvent(sock->rawFd(), EventPoller::Event_Read | EventPoller::Event_Error | EventPoller::Event_Write, [weak_self,weak_sock,is_udp](int event) {
        auto strong_self = weak_self.lock();
        auto strong_sock = weak_sock.lock();
        if (!strong_self || !strong_sock) {
            return;
        }

        if (event & EventPoller::Event_Read) {
            strong_self->onRead(strong_sock, is_udp);
        }
        if (event & EventPoller::Event_Write) {
            strong_self->onWriteAble(strong_sock);
        }
        if (event & EventPoller::Event_Error) {
            strong_self->emitErr(getSockErr(strong_sock));
        }
    });

    return -1 != result;
}

ssize_t Socket::onRead(const SockFD::Ptr &sock, bool is_udp) noexcept{
    ssize_t ret = 0, nread = 0;
    auto sock_fd = sock->rawFd();

    auto data = _read_buffer->data();
    //最后一个字节设置为'\0'
    auto capacity = _read_buffer->getCapacity() - 1;

    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);

    while (_enable_recv) {
        do {
            nread = recvfrom(sock_fd, data, capacity, 0, (struct sockaddr *)&addr, &len);
        } while (-1 == nread && UV_EINTR == get_uv_error(true));

        if (nread == 0) {
            if (!is_udp) {
                emitErr(SockException(Err_eof, "end of file"));
            } else {
                WarnL << "recv eof on udp socket[" << sock_fd << "]";
            }
            return ret;
        }

        if (nread == -1) {
            auto err = get_uv_error(true);
            if (err != UV_EAGAIN) {
                if (!is_udp) {
                    emitErr(toSockException(err));
                } else {
                    WarnL << "recv err on udp socket[" << sock_fd << "]:" << uv_strerror(err);
                }
            }
            return ret;
        }

        ret += nread;
        data[nread] = '\0';
        //设置buffer有效数据大小
        _read_buffer->setSize(nread);

        //触发回调
        LOCK_GUARD(_mtx_event);
        try {
            //此处捕获异常，目的是防止数据未读尽，epoll边沿触发失效的问题
            _on_read(_read_buffer, (struct sockaddr *)&addr, len);
        } catch (std::exception &ex) {
            ErrorL << "触发socket on_read事件时,捕获到异常:" << ex.what();
        }
    }
    return 0;
}

bool Socket::emitErr(const SockException& err) noexcept{
    {
        LOCK_GUARD(_mtx_sock_fd);
        if (!_sock_fd) {
            //防止多次触发onErr事件
            return false;
        }
    }

    closeSock();

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
            ErrorL << "触发socket on_err事件时,捕获到异常:" << ex.what();
        }
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
        return send_l(std::move(buf), false, try_flush);
    }
    return send_l(std::make_shared<BufferSock>(std::move(buf), addr, addr_len), true, try_flush);
}

ssize_t Socket::send_l(Buffer::Ptr buf, bool is_buf_sock, bool try_flush) {
    auto size = buf ? buf->size() : 0;
    if (!size) {
        return 0;
    }

    SockFD::Ptr sock;
    {
        LOCK_GUARD(_mtx_sock_fd);
        sock = _sock_fd;
    }

    if (!sock) {
        //如果已断开连接或者发送超时
        return -1;
    }

    {
        LOCK_GUARD(_mtx_send_buf_waiting);
        _send_buf_waiting.emplace_back(std::move(buf), is_buf_sock);
    }

    if(try_flush){
        if (_sendable) {
            //该socket可写
            return flushData(sock, false) ? size : -1;
        }

        //该socket不可写,判断发送超时
        if (_send_flush_ticker.elapsedTime() > _max_send_buffer_ms) {
            //如果发送列队中最老的数据距今超过超时时间限制，那么就断开socket连接
            emitErr(SockException(Err_other, "socket send timeout"));
            return -1;
        }
    }

    return size;
}

void Socket::onFlushed(const SockFD::Ptr &pSock) {
    bool flag;
    {
        LOCK_GUARD(_mtx_event);
        flag = _on_flush();
    }
    if (!flag) {
        setOnFlush(nullptr);
    }
}

void Socket::closeSock() {
    _con_timer = nullptr;
    _async_con_cb = nullptr;

    LOCK_GUARD(_mtx_sock_fd);
    _sock_fd  = nullptr;
}

size_t Socket::getSendBufferCount(){
    size_t ret = 0;
    {
        LOCK_GUARD(_mtx_send_buf_waiting);
        ret += _send_buf_waiting.size();
    }

    {
        LOCK_GUARD(_mtx_send_buf_sending);
        _send_buf_sending.for_each([&](BufferList::Ptr &buf) {
            ret += buf->count();
        });
    }
    return ret;
}

uint64_t Socket::elapsedTimeAfterFlushed(){
    return _send_flush_ticker.elapsedTime();
}

bool Socket::listen(const SockFD::Ptr &sock){
    closeSock();
    weak_ptr<SockFD> weak_sock = sock;
    weak_ptr<Socket> weak_self = shared_from_this();
    _enable_recv = true;
    int result = _poller->addEvent(sock->rawFd(), EventPoller::Event_Read | EventPoller::Event_Error, [weak_self, weak_sock](int event) {
        auto strong_self = weak_self.lock();
        auto strong_sock = weak_sock.lock();
        if (!strong_self || !strong_sock) {
            return;
        }
        strong_self->onAccept(strong_sock, event);
    });

    if (result == -1) {
        return false;
    }

    LOCK_GUARD(_mtx_sock_fd);
    _sock_fd = sock;
    return true;
}

bool Socket::listen(uint16_t port, const string &local_ip, int backlog) {
    int sock = SockUtil::listen(port, local_ip.data(), backlog);
    if (sock == -1) {
        return false;
    }
    return listen(makeSock(sock, SockNum::Sock_TCP));
}

bool Socket::bindUdpSock(uint16_t port, const string &local_ip, bool enable_reuse) {
    closeSock();
    int fd = SockUtil::bindUdpSock(port, local_ip.data(), enable_reuse);
    if (fd == -1) {
        return false;
    }
    auto sock = makeSock(fd, SockNum::Sock_UDP);
    if (!attachEvent(sock, true)) {
        return false;
    }
    LOCK_GUARD(_mtx_sock_fd);
    _sock_fd = sock;
    return true;
}

int Socket::onAccept(const SockFD::Ptr &sock, int event) noexcept {
    int fd;
    while (true) {
        if (event & EventPoller::Event_Read) {
            do {
                fd = (int)accept(sock->rawFd(), nullptr, nullptr);
            } while (-1 == fd && UV_EINTR == get_uv_error(true));

            if (fd == -1) {
                int err = get_uv_error(true);
                if (err == UV_EAGAIN) {
                    //没有新连接
                    return 0;
                }
                auto ex = toSockException(err);
                emitErr(ex);
                ErrorL << "tcp服务器监听异常:" << ex.what();
                return -1;
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
                //此处捕获异常，目的是防止socket未accept尽，epoll边沿触发失效的问题
                LOCK_GUARD(_mtx_event);
                //拦截Socket对象的构造
                peer_sock = _on_before_accept(_poller);
            } catch (std::exception &ex) {
                ErrorL << "触发socket before accept事件时,捕获到异常:" << ex.what();
                close(fd);
                continue;
            }

            if (!peer_sock) {
                //此处是默认构造行为，也就是子Socket共用父Socket的poll线程并且关闭互斥锁
                peer_sock = Socket::createSocket(_poller, false);
            }

            //设置好fd,以备在onAccept事件中可以正常访问该fd
            auto peer_sock_fd = peer_sock->setPeerSock(fd);

            shared_ptr<void> completed(nullptr, [peer_sock, peer_sock_fd](void *) {
                try {
                    //然后把该fd加入poll监听(确保先触发onAccept事件然后再触发onRead等事件)
                    if (!peer_sock->attachEvent(peer_sock_fd, false)) {
                        //加入poll监听失败，触发onErr事件，通知该Socket无效
                        peer_sock->emitErr(SockException(Err_eof, "add event to poller failed when accept a socket"));
                    }
                } catch (std::exception &ex) {
                    ErrorL << ex.what();
                }
            });

            try {
                //此处捕获异常，目的是防止socket未accept尽，epoll边沿触发失效的问题
                LOCK_GUARD(_mtx_event);
                //先触发onAccept事件，此时应该监听该Socket的onRead等事件
                _on_accept(peer_sock, completed);
            } catch (std::exception &ex) {
                ErrorL << "触发socket accept事件时,捕获到异常:" << ex.what();
                continue;
            }
        }

        if (event & EventPoller::Event_Error) {
            auto ex = getSockErr(sock);
            emitErr(ex);
            ErrorL << "tcp服务器监听异常:" << ex.what();
            return -1;
        }
    }
}

SockFD::Ptr Socket::setPeerSock(int fd) {
    closeSock();
    auto sock = makeSock(fd, SockNum::Sock_TCP);
    LOCK_GUARD(_mtx_sock_fd);
    _sock_fd = sock;
    return sock;
}

string Socket::get_local_ip() {
    LOCK_GUARD(_mtx_sock_fd);
    if (!_sock_fd) {
        return "";
    }
    return SockUtil::get_local_ip(_sock_fd->rawFd());
}

uint16_t Socket::get_local_port() {
    LOCK_GUARD(_mtx_sock_fd);
    if (!_sock_fd) {
        return 0;
    }
    return SockUtil::get_local_port(_sock_fd->rawFd());
}

string Socket::get_peer_ip() {
    LOCK_GUARD(_mtx_sock_fd);
    if (!_sock_fd) {
        return "";
    }
    return SockUtil::get_peer_ip(_sock_fd->rawFd());
}

uint16_t Socket::get_peer_port() {
    LOCK_GUARD(_mtx_sock_fd);
    if (!_sock_fd) {
        return 0;
    }
    return SockUtil::get_peer_port(_sock_fd->rawFd());
}

string Socket::getIdentifier() const{
    static string class_name = "Socket:";
    return class_name + to_string(reinterpret_cast<uint64_t>(this));
}

bool Socket::flushData(const SockFD::Ptr &sock, bool poller_thread) {
    decltype(_send_buf_sending) send_buf_sending_tmp;
    {
        //转移出二级缓存
        LOCK_GUARD(_mtx_send_buf_sending);
        if(!_send_buf_sending.empty()){
            send_buf_sending_tmp.swap(_send_buf_sending);
        }
    }

    if (send_buf_sending_tmp.empty()) {
        _send_flush_ticker.resetTime();
        do {
            {
                //二级发送缓存为空，那么我们接着消费一级缓存中的数据
                LOCK_GUARD(_mtx_send_buf_waiting);
                if (!_send_buf_waiting.empty()) {
                    //把一级缓中数数据放置到二级缓存中并清空
                    LOCK_GUARD(_mtx_event);
                    send_buf_sending_tmp.emplace_back(BufferList::create(std::move(_send_buf_waiting), _send_result, sock->type() == SockNum::Sock_UDP));
                    break;
                }
            }
            //如果一级缓存也为空,那么说明所有数据均写入socket了
            if (poller_thread) {
                //poller线程触发该函数，那么该socket应该已经加入了可写事件的监听；
                //那么在数据列队清空的情况下，我们需要关闭监听以免触发无意义的事件回调
                stopWriteAbleEvent(sock);
                onFlushed(sock);
            }
            return true;
        } while (0);
    }

    int fd = sock->rawFd();
    bool is_udp = sock->type() == SockNum::Sock_UDP;
    while (!send_buf_sending_tmp.empty()) {
        auto &packet = send_buf_sending_tmp.front();
        auto n = packet->send(fd, _sock_flags);
        if (n > 0) {
            //全部或部分发送成功
            if (packet->empty()) {
                //全部发送成功
                send_buf_sending_tmp.pop_front();
                continue;
            }
            //部分发送成功
            if (!poller_thread) {
                //如果该函数是poller线程触发的，那么该socket应该已经加入了可写事件的监听，所以我们不需要再次加入监听
                startWriteAbleEvent(sock);
            }
            break;
        }

        //一个都没发送成功
        int err = get_uv_error(true);
        if (err == UV_EAGAIN) {
            //等待下一次发送
            if (!poller_thread) {
                //如果该函数是poller线程触发的，那么该socket应该已经加入了可写事件的监听，所以我们不需要再次加入监听
                startWriteAbleEvent(sock);
            }
            break;
        }

        //其他错误代码，发生异常
        if (is_udp) {
            // udp发送异常，把数据丢弃
            send_buf_sending_tmp.pop_front();
            WarnL << "send udp socket[" << fd << "] failed, data ignored:" << uv_strerror(err);
            continue;
        }
        // tcp发送失败时，触发异常
        emitErr(toSockException(err));
        return false;
    }

    //回滚未发送完毕的数据
    if (!send_buf_sending_tmp.empty()) {
        //有剩余数据
        LOCK_GUARD(_mtx_send_buf_sending);
        send_buf_sending_tmp.swap(_send_buf_sending);
        _send_buf_sending.append(send_buf_sending_tmp);
        //二级缓存未全部发送完毕，说明该socket不可写，直接返回
        return true;
    }

    //二级缓存已经全部发送完毕，说明该socket还可写，我们尝试继续写
    //如果是poller线程，我们尝试再次写一次(因为可能其他线程调用了send函数又有新数据了)
    return poller_thread ? flushData(sock, poller_thread) : true;
}

void Socket::onWriteAble(const SockFD::Ptr &sock) {
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
        //数据已经清空了，我们停止监听可写事件
        stopWriteAbleEvent(sock);
    } else {
        //socket可写，我们尝试发送剩余的数据
        flushData(sock, true);
    }
}

void Socket::startWriteAbleEvent(const SockFD::Ptr &sock) {
    //开始监听socket可写事件
    _sendable = false;
    int flag = _enable_recv ? EventPoller::Event_Read : 0;
    _poller->modifyEvent(sock->rawFd(), flag | EventPoller::Event_Error | EventPoller::Event_Write);
}

void Socket::stopWriteAbleEvent(const SockFD::Ptr &sock) {
    //停止监听socket可写事件
    _sendable = true;
    int flag = _enable_recv ? EventPoller::Event_Read : 0;
    _poller->modifyEvent(sock->rawFd(), flag | EventPoller::Event_Error);
}

void Socket::enableRecv(bool enabled) {
    if (_enable_recv == enabled) {
        return;
    }
    _enable_recv = enabled;
    int read_flag = _enable_recv ? EventPoller::Event_Read : 0;
    //可写时，不监听可写事件
    int send_flag = _sendable ? 0 : EventPoller::Event_Write;
    _poller->modifyEvent(rawFD(), read_flag | send_flag | EventPoller::Event_Error);
}

SockFD::Ptr Socket::makeSock(int sock, SockNum::SockType type) {
    return std::make_shared<SockFD>(sock, type, _poller);
}

int Socket::rawFD() const{
    LOCK_GUARD(_mtx_sock_fd);
    if (!_sock_fd) {
        return -1;
    }
    return _sock_fd->rawFd();
}

SockNum::SockType Socket::sockType() const {
    LOCK_GUARD(_mtx_sock_fd);
    if (!_sock_fd) {
        return SockNum::Sock_Invalid;
    }
    return _sock_fd->type();
}

void Socket::setSendTimeOutSecond(uint32_t second){
    _max_send_buffer_ms = second * 1000;
}

bool Socket::isSocketBusy() const{
    return !_sendable.load();
}

const EventPoller::Ptr &Socket::getPoller() const{
    return _poller;
}

bool Socket::cloneFromListenSocket(const Socket &other){
    SockFD::Ptr sock;
    {
        LOCK_GUARD(other._mtx_sock_fd);
        if (!other._sock_fd) {
            WarnL << "sockfd of src socket is null!";
            return false;
        }
        sock = std::make_shared<SockFD>(*(other._sock_fd), _poller);
    }
    return listen(sock);
}

bool Socket::bindPeerAddr(const struct sockaddr *dst_addr, socklen_t addr_len) {
    LOCK_GUARD(_mtx_sock_fd);
    if (!_sock_fd) {
        return false;
    }
    if (_sock_fd->type() != SockNum::Sock_UDP) {
        return false;
    }
    if (-1 == ::connect(_sock_fd->rawFd(), dst_addr, addr_len ? addr_len : SockUtil::get_sock_len(dst_addr))) {
        WarnL << "connect peer address failed:" << SockUtil::inet_ntoa(dst_addr);
        return false;
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

SocketHelper::~SocketHelper() {}

void SocketHelper::setPoller(const EventPoller::Ptr &poller){
    _poller = poller;
}

void SocketHelper::setSock(const Socket::Ptr &sock) {
    _peer_port = 0;
    _local_port = 0;
    _peer_ip.clear();
    _local_ip.clear();
    _sock = sock;
    if (_sock) {
        _poller = _sock->getPoller();
    }
}

const EventPoller::Ptr& SocketHelper::getPoller() const {
    assert(_poller);
    return _poller;
}

const Socket::Ptr& SocketHelper::getSock() const{
    return _sock;
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

string SocketHelper::get_local_ip() {
    if (_sock && _local_ip.empty()) {
        _local_ip = _sock->get_local_ip();
    }
    return _local_ip;
}

uint16_t SocketHelper::get_local_port() {
    if (_sock && _local_port == 0) {
        _local_port = _sock->get_local_port();
    }
    return _local_port;
}

string SocketHelper::get_peer_ip() {
    if (_sock && _peer_ip.empty()) {
        _peer_ip = _sock->get_peer_ip();
    }
    return _peer_ip;
}

uint16_t SocketHelper::get_peer_port() {
    if (_sock && _peer_port == 0) {
        _peer_port = _sock->get_peer_port();
    }
    return _peer_port;
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

void SocketHelper::setOnCreateSocket(Socket::onCreateSocket cb){
    if (cb) {
        _on_create_socket = std::move(cb);
    } else {
        _on_create_socket = [](const EventPoller::Ptr &poller) {
            return Socket::createSocket(poller, false);
        };
    }
}

Socket::Ptr SocketHelper::createSocket(){
    return _on_create_socket(_poller);
}

std::ostream &operator<<(std::ostream &ost, const SockException &err) {
    ost << err.getErrCode() << "(" << err.what() << ")";
    return ost;
}

}  // namespace toolkit



