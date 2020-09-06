/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
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

Socket::Socket(const EventPoller::Ptr &poller, bool enable_mutex) :
        _mtx_sock_fd(enable_mutex), _mtx_send_buf_waiting(enable_mutex),
        _mtx_send_buf_sending(enable_mutex), _mtx_event(enable_mutex) {

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
            WarnL << "Socket not set readCB";
        };
    }
}

void Socket::setOnErr(onErrCB cb) {
    LOCK_GUARD(_mtx_event);
    if (cb) {
        _on_err = std::move(cb);
    } else {
        _on_err = [](const SockException &err) {
            WarnL << "Socket not set errCB";
        };
    }
}

void Socket::setOnAccept(onAcceptCB cb) {
    LOCK_GUARD(_mtx_event);
    if (cb) {
        _on_accept = std::move(cb);
    } else {
        _on_accept = [](Socket::Ptr &sock) {
            WarnL << "Socket not set acceptCB";
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

void Socket::setOnBeforeAccept(onBeforeAcceptCB cb){
    LOCK_GUARD(_mtx_event);
    if (cb) {
        _on_before_accept = std::move(cb);
    } else {
        _on_before_accept = [](const EventPoller::Ptr &poller) {
            return nullptr;
        };
    }
}

#define CLOSE_SOCK(fd) if(fd != -1) {close(fd);}

void Socket::connect(const string &url, uint16_t port, onErrCB con_cb_in, float timeout_sec, const string &local_ip, uint16_t local_port) {
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
        int result = strong_self->_poller->addEvent(sock, Event_Write, [weak_self, weak_sock_fd, con_cb](int event) {
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

    auto poller = _poller;
    weak_ptr<function<void(int)> > weak_task = async_con_cb;

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

    //连接超时定时器
    _con_timer = std::make_shared<Timer>(timeout_sec, [weak_self, con_cb]() {
        con_cb(SockException(Err_timeout, uv_strerror(UV_ETIMEDOUT)));
        return false;
    }, _poller);

    _async_con_cb = async_con_cb;
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

    switch (error) {
        case 0:
        case UV_EAGAIN: return SockException(Err_success, "success");
        case UV_ECONNREFUSED: return SockException(Err_refused, uv_strerror(error), error);
        case UV_ETIMEDOUT: return SockException(Err_timeout, uv_strerror(error), error);
        default: return SockException(Err_other, uv_strerror(error), error);
    }
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
    if(!_read_buffer){
        //udp包最大能达到64KB
        _read_buffer = std::make_shared<BufferRaw>(is_udp ? 0xFFFF : 128 * 1024);
    }
    int result = _poller->addEvent(sock->rawFd(), Event_Read | Event_Error | Event_Write, [weak_self,weak_sock,is_udp](int event) {
        auto strong_self = weak_self.lock();
        auto strong_sock = weak_sock.lock();
        if (!strong_self || !strong_sock) {
            return;
        }

        if (event & Event_Read) {
            strong_self->onRead(strong_sock, is_udp);
        }
        if (event & Event_Write) {
            strong_self->onWriteAble(strong_sock);
        }
        if (event & Event_Error) {
            strong_self->onError(strong_sock);
        }
    });

    return -1 != result;
}

void Socket::setReadBuffer(const BufferRaw::Ptr &buffer){
    if (!buffer || buffer->getCapacity() < 2) {
        return;
    }
    weak_ptr<Socket> weak_self = shared_from_this();
    _poller->async([buffer, weak_self]() {
        auto strong_self = weak_self.lock();
        if (strong_self) {
            strong_self->_read_buffer = std::move(buffer);
        }
    });
}

int Socket::onRead(const SockFD::Ptr &sock, bool is_udp) {
    int ret = 0, nread = 0, sock_fd = sock->rawFd();

    //保存_read_buffer的临时变量，防止onRead事件中修改
    auto buffer = _read_buffer;
    auto data = buffer->data();
    //最后一个字节设置为'\0'
    auto capacity = buffer->getCapacity() - 1;

    struct sockaddr addr;
    socklen_t len = sizeof(struct sockaddr);

    while (_enable_recv) {
        do {
            nread = recvfrom(sock_fd, data, capacity, 0, &addr, &len);
        } while (-1 == nread && UV_EINTR == get_uv_error(true));

        if (nread == 0) {
            if (!is_udp) {
                emitErr(SockException(Err_eof, "end of file"));
            }
            return ret;
        }

        if (nread == -1) {
            if (get_uv_error(true) != UV_EAGAIN) {
                onError(sock);
            }
            return ret;
        }

        ret += nread;
        data[nread] = '\0';
        //设置buffer有效数据大小
        buffer->setSize(nread);

        //触发回调
        LOCK_GUARD(_mtx_event);
        _on_read(buffer, &addr, len);
    }
    return 0;
}

void Socket::onError(const SockFD::Ptr &sock) {
    emitErr(getSockErr(sock));
}

bool Socket::emitErr(const SockException& err) {
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
        strong_self->_on_err(err);
    });

    return true;
}

int Socket::send(const char *buf, int size, struct sockaddr *addr, socklen_t addr_len, bool try_flush) {
    if (size <= 0) {
        size = strlen(buf);
        if (!size) {
            return 0;
        }
    }
    BufferRaw::Ptr ptr = obtainBuffer();
    ptr->assign(buf, size);
    return send(ptr, addr, addr_len, try_flush);
}

int Socket::send(const string &buf, struct sockaddr *addr, socklen_t addr_len, bool try_flush) {
    return send(std::make_shared<BufferString>(buf), addr, addr_len, try_flush);
}

int Socket::send(string &&buf, struct sockaddr *addr, socklen_t addr_len, bool try_flush) {
    return send(std::make_shared<BufferString>(std::move(buf)), addr, addr_len, try_flush);
}

int Socket::send(const Buffer::Ptr &buf , struct sockaddr *addr, socklen_t addr_len, bool try_flush){
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
        _send_buf_waiting.emplace_back(sock->type() == SockNum::Sock_UDP ? std::make_shared<BufferSock>(buf, addr, addr_len) : buf);
    }

    if(try_flush){
        if (_sendable) {
            //该socket可写
            return flushData(sock, false) ? size : -1;
        }

        //该socket不可写,判断发送超时
        if (_send_flush_ticker.elapsedTime() > _max_send_buffer_ms) {
            //如果发送列队中最老的数据距今超过超时时间限制，那么就断开socket连接
            emitErr(SockException(Err_other, "Socket send timeout"));
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

int Socket::getSendBufferCount(){
    int ret = 0;
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
    int result = _poller->addEvent(sock->rawFd(), Event_Read | Event_Error, [weak_self, weak_sock](int event) {
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

bool Socket::bindUdpSock(uint16_t port, const string &local_ip) {
    closeSock();
    int fd = SockUtil::bindUdpSock(port, local_ip.data());
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

int Socket::onAccept(const SockFD::Ptr &sock, int event) {
    int fd;
    while (true) {
        if (event & Event_Read) {
            do {
                fd = accept(sock->rawFd(), NULL, NULL);
            } while (-1 == fd && UV_EINTR == get_uv_error(true));

            if (fd == -1) {
                int err = get_uv_error(true);
                if (err == UV_EAGAIN) {
                    //没有新连接
                    return 0;
                }
                ErrorL << "tcp服务器监听异常:" << uv_strerror(err);
                onError(sock);
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
            {
                //拦截Socket对象的构造
                LOCK_GUARD(_mtx_event);
                peer_sock = _on_before_accept(_poller);
            }

            if (!peer_sock) {
                //此处是默认构造行为，也就是子Socket共用父Socket的poll线程并且关闭互斥锁
                peer_sock = std::make_shared<Socket>(_poller, false);
            }

            //设置好fd,以备在onAccept事件中可以正常访问该fd
            auto peer_sock_fd = peer_sock->setPeerSock(fd);

            {
                //先触发onAccept事件，此时应该监听该Socket的onRead等事件
                LOCK_GUARD(_mtx_event);
                _on_accept(peer_sock);
            }

            //然后把该fd加入poll监听(确保先触发onAccept事件然后再触发onRead等事件)
            if (!peer_sock->attachEvent(peer_sock_fd, false)) {
                //加入poll监听失败，触发onErr事件，通知该Socket无效
                peer_sock->emitErr(SockException(Err_eof, "add event to poller failed when accept a socket"));
            }
        }

        if (event & Event_Error) {
            ErrorL << "tcp服务器监听异常:" << get_uv_errmsg();
            onError(sock);
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
                    send_buf_sending_tmp.emplace_back(std::make_shared<BufferList>(_send_buf_waiting));
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
        int n = packet->send(fd, _sock_flags, is_udp);
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
        onError(sock);
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
    int flag = _enable_recv ? Event_Read : 0;
    _poller->modifyEvent(sock->rawFd(), flag | Event_Error | Event_Write);
}

void Socket::stopWriteAbleEvent(const SockFD::Ptr &sock) {
    //停止监听socket可写事件
    _sendable = true;
    int flag = _enable_recv ? Event_Read : 0;
    _poller->modifyEvent(sock->rawFd(), flag | Event_Error);
}

void Socket::enableRecv(bool enabled) {
    if (_enable_recv == enabled) {
        return;
    }
    _enable_recv = enabled;
    int read_flag = _enable_recv ? Event_Read : 0;
    //可写时，不监听可写事件
    int send_flag = _sendable ? 0 : Event_Write;
    _poller->modifyEvent(rawFD(), read_flag | send_flag | Event_Error);
}

SockFD::Ptr Socket::makeSock(int sock,SockNum::SockType type){
    return std::make_shared<SockFD>(sock, type, _poller);
}

int Socket::rawFD() const{
    LOCK_GUARD(_mtx_sock_fd);
    if (!_sock_fd) {
        return -1;
    }
    return _sock_fd->rawFd();
}

void Socket::setSendTimeOutSecond(uint32_t second){
    _max_send_buffer_ms = second * 1000;
}

BufferRaw::Ptr Socket::obtainBuffer() {
    return std::make_shared<BufferRaw>();//_bufferPool.obtain();
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

bool Socket::setSendPeerAddr(const struct sockaddr *dst_addr, socklen_t addr_len) {
    LOCK_GUARD(_mtx_sock_fd);
    if (!_sock_fd) {
        return false;
    }
    if (_sock_fd->type() != SockNum::Sock_UDP) {
        return false;
    }
    if (!addr_len) {
        addr_len = sizeof(struct sockaddr);
    }
    return 0 == ::connect(_sock_fd->rawFd(), dst_addr, addr_len);
}

void Socket::setSendFlags(int flags) {
    _sock_flags = flags;
}

///////////////SockSender///////////////////

SockSender &SockSender::operator<<(const char *buf) {
    send(buf);
    return *this;
}

SockSender &SockSender::operator<<(const string &buf) {
    send(buf);
    return *this;
}

SockSender &SockSender::operator<<(string &&buf) {
    send(std::move(buf));
    return *this;
}

SockSender &SockSender::operator<<(const Buffer::Ptr &buf) {
    send(buf);
    return *this;
}

int SockSender::send(const string &buf) {
    auto buffer = std::make_shared<BufferString>(buf);
    return send(buffer);
}

int SockSender::send(string &&buf) {
    auto buffer = std::make_shared<BufferString>(std::move(buf));
    return send(buffer);
}

int SockSender::send(const char *buf, int size) {
    auto buffer = std::make_shared<BufferRaw>();
    buffer->assign(buf, size);
    return send(buffer);
}

///////////////SocketHelper///////////////////

SocketHelper::SocketHelper(const Socket::Ptr &sock) {
    setSock(sock);
}

SocketHelper::~SocketHelper() {}

void SocketHelper::setSock(const Socket::Ptr &sock) {
    _sock = sock;
    if (_sock) {
        _poller = _sock->getPoller();
    }
}

EventPoller::Ptr SocketHelper::getPoller() {
    return _poller;
}

int SocketHelper::send(const Buffer::Ptr &buf) {
    if (!_sock) {
        return -1;
    }
    return _sock->send(buf, nullptr, 0, _try_flush);
}

BufferRaw::Ptr SocketHelper::obtainBuffer(const void *data, int len) {
    BufferRaw::Ptr buffer;
    if (!_sock) {
        buffer = std::make_shared<BufferRaw>();
    } else {
        buffer = _sock->obtainBuffer();
    }
    if (data && len) {
        buffer->assign((const char *) data, len);
    }
    return buffer;
};

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

}  // namespace toolkit



