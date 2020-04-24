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

Socket::Socket(const EventPoller::Ptr &poller,bool enableMutex) :
        _mtx_sockFd(enableMutex),
        _mtx_bufferWaiting(enableMutex),
        _mtx_bufferSending(enableMutex),
        _mtx_event(enableMutex){
    _poller = poller;
    if(!_poller){
        _poller = EventPollerPool::Instance().getPoller();
    }
    _canSendSock = true;
    setOnRead(nullptr);
    setOnErr(nullptr);
    setOnAccept(nullptr);
    setOnFlush(nullptr);
    setOnBeforeAccept(nullptr);
}
Socket::~Socket() {
    closeSock();
    //TraceL << endl;
}

void Socket::setOnRead(const onReadCB &cb) {
    LOCK_GUARD(_mtx_event);
    if (cb) {
        _readCB = cb;
    } else {
        _readCB = [](const Buffer::Ptr &buf,struct sockaddr * , int) {
            WarnL << "Socket not set readCB";
        };
    }
}

void Socket::setOnErr(const onErrCB &cb) {
    LOCK_GUARD(_mtx_event);
    if (cb) {
        _errCB = cb;
    } else {
        _errCB = [](const SockException &err) {
            WarnL << "Socket not set errCB";
        };
    }
}
void Socket::setOnAccept(const onAcceptCB &cb) {
    LOCK_GUARD(_mtx_event);
    if (cb) {
        _acceptCB = cb;
    } else {
        _acceptCB = [](Socket::Ptr &sock) {
            WarnL << "Socket not set acceptCB";
        };
    }
}

void Socket::setOnFlush(const onFlush &cb) {
    LOCK_GUARD(_mtx_event);
    if (cb) {
        _flushCB = cb;
    } else {
        _flushCB = []() {return true;};
    }
}

//设置Socket生成拦截器
void Socket::setOnBeforeAccept(const onBeforeAcceptCB &cb){
    LOCK_GUARD(_mtx_event);
    if (cb) {
        _beforeAcceptCB = cb;
    } else {
        _beforeAcceptCB = [](const EventPoller::Ptr &poller) {
            return nullptr;
        };
    }
}

void Socket::connect(const string &url, uint16_t port,const onErrCB &connectCB, float timeoutSec,const char *localIp,uint16_t localPort) {
    //重置当前socket
    closeSock();

    auto poller = _poller;
    weak_ptr<Socket> weakSelf = shared_from_this();
    //是否已经触发连接超时回调
    shared_ptr<bool> timeOuted = std::make_shared<bool>(false);

    auto asyncConnectCB = std::make_shared<function<void(int)> >([poller,weakSelf,connectCB,timeOuted](int sock){
        poller->async([weakSelf,connectCB,timeOuted,sock](){
            auto strongSelf = weakSelf.lock();
            if(!strongSelf || *timeOuted ){
                //本对象已经销毁或已经超时回调
                if(sock != -1){
                    close(sock);
                }
                return;
            }

            if(sock == -1){
                //发起连接服务器失败，一般都是dns解析失败导致
                connectCB(SockException(Err_dns, get_uv_errmsg(true)));
                //取消超时定时器
                strongSelf->_conTimer.reset();
                return;
            }

            auto sockFD = strongSelf->makeSock(sock,SockNum::Sock_TCP);
            weak_ptr<SockFD> weakSock = sockFD;
            //监听该socket是否可写，可写表明已经连接服务器成功
            int result  = strongSelf->_poller->addEvent(sock, Event_Write, [weakSelf,weakSock,connectCB,timeOuted](int event) {
                auto strongSelf = weakSelf.lock();
                auto strongSock = weakSock.lock();
                if(!strongSelf || !strongSock || *timeOuted) {
                    //自己或该Socket已经被销毁或已经触发超时回调
                    return;
                }
                //socket可写事件，说明已经连接服务器成功
                strongSelf->onConnected(strongSock,connectCB);
            });
            if(result == -1){
                WarnL << "开始Poll监听失败";
                connectCB(SockException(Err_other,"开始Poll监听失败"));
                strongSelf->_conTimer.reset();
                return;
            }
            //保存fd
            LOCK_GUARD(strongSelf->_mtx_sockFd);
            strongSelf->_sockFd = sockFD;
        });
    });

    weak_ptr<function<void(int)> > weakTask = asyncConnectCB;
    _asyncConnectCB = asyncConnectCB;

    //DNS解析放在后台线程执行
    string strLocalIp = localIp;
    WorkThreadPool::Instance().getExecutor()->async([url,port,strLocalIp,localPort,weakTask](){
        //注释式dns解析，并且异步连接服务器
        int sock = SockUtil::connect(url.data(), port, true, strLocalIp.data(), localPort);
        auto strongTask = weakTask.lock();
        if(strongTask){
            (*strongTask)(sock);
        } else if(sock != -1){
            //本次连接被取消
            close(sock);
        }
    });

    //连接超时定时器
    _conTimer = std::make_shared<Timer>(timeoutSec, [weakSelf,connectCB,timeOuted]() {
        auto strongSelf = weakSelf.lock();
        if(!strongSelf) {
            //自己已经销毁
            return false;
        }
        *timeOuted = true;
        SockException err(Err_timeout, uv_strerror(UV_ETIMEDOUT));
        strongSelf->emitErr(err);
        connectCB(err);
        return false;
    },_poller, false);

}

void Socket::onConnected(const SockFD::Ptr &pSock,const onErrCB &connectCB) {
    _conTimer.reset();
    auto err = getSockErr(pSock, false);
    do {
        if (!err) {
            _poller->delEvent(pSock->rawFd());
            if (!attachEvent(pSock, false)) {
                WarnL << "开始Poll监听失败";
                err.reset(Err_other, "开始Poll监听失败");
                break;
            }
            pSock->setConnected();
            connectCB(err);
            return;
        }
    }while(0);

    emitErr(err);
    connectCB(err);
}

SockException Socket::getSockErr(const SockFD::Ptr &sock, bool tryErrno) {
    int error = 0, len;
    len = sizeof(int);
    getsockopt(sock->rawFd(), SOL_SOCKET, SO_ERROR, (char *)&error, (socklen_t *) &len);
    if (error == 0) {
        if(tryErrno){
            error = get_uv_error(true);
        }
    }else {
        error = uv_translate_posix_error(error);
    }

    switch (error) {
    case 0:
    case UV_EAGAIN:
        return SockException(Err_success, "success");
    case UV_ECONNREFUSED:
        return SockException(Err_refused, uv_strerror(error),error);
    case UV_ETIMEDOUT:
        return SockException(Err_timeout, uv_strerror(error),error);
    default:
        return SockException(Err_other, uv_strerror(error),error);
    }
}
bool Socket::attachEvent(const SockFD::Ptr &pSock,bool isUdp) {
    weak_ptr<Socket> weakSelf = shared_from_this();
    weak_ptr<SockFD> weakSock = pSock;
    _enableRecv = true;
    if(!_readBuffer){
        //udp包最大能达到64KB
        _readBuffer = std::make_shared<BufferRaw>(isUdp ? 0xFFFF : 128 * 1024);
    }
    int result = _poller->addEvent(pSock->rawFd(), Event_Read | Event_Error | Event_Write, [weakSelf,weakSock,isUdp](int event) {
        auto strongSelf = weakSelf.lock();
        auto strongSock = weakSock.lock();
        if(!strongSelf || !strongSock) {
            return;
        }
        if (event & Event_Error) {
            strongSelf->onError(strongSock);
            return;
        }
        if (event & Event_Read) {
            strongSelf->onRead(strongSock,isUdp);
        }
        if (event & Event_Write) {
            strongSelf->onWriteAble(strongSock);
        }
    });

    return -1 != result;
}

void Socket::setReadBuffer(const BufferRaw::Ptr &readBuffer){
    if(!readBuffer || readBuffer->getCapacity() < 2){
        return;
    }
    weak_ptr<Socket> weakSelf = shared_from_this();
    _poller->async([readBuffer,weakSelf](){
        auto strongSelf = weakSelf.lock();
        if(strongSelf){
            strongSelf->_readBuffer = std::move(readBuffer);
        }
    });
}
int Socket::onRead(const SockFD::Ptr &pSock,bool isUdp) {
    int ret = 0 , nread = 0 ,sock = pSock->rawFd();
    struct sockaddr peerAddr;
    socklen_t len = sizeof(struct sockaddr);
    //保存_readBuffer的临时变量，防止onRead事件中设置_readBuffer
    auto readBuffer = _readBuffer;
    auto data = readBuffer->data();
    //最后一个字节设置为'\0'
    auto capacity =  readBuffer->getCapacity() - 1;

    while (_enableRecv) {
        do {
            nread = recvfrom(sock, data, capacity, 0, &peerAddr, &len);
        } while (-1 == nread && UV_EINTR == get_uv_error(true));

        if (nread == 0) {
            if (!isUdp) {
                emitErr(SockException(Err_eof, "end of file"));
            }
            return ret;
        }

        if (nread == -1) {
            if (get_uv_error(true) != UV_EAGAIN) {
                onError(pSock);
            }
            return ret;
        }

        ret += nread;
        data[nread] = '\0';
        //设置buffer有效数据大小
        readBuffer->setSize(nread);

        //触发回调
        LOCK_GUARD(_mtx_event);
        _readCB(readBuffer, &peerAddr, len);
    }
    return 0;
}
void Socket::onError(const SockFD::Ptr &pSock) {
    emitErr(getSockErr(pSock));
}
bool Socket::emitErr(const SockException& err) {
    {
        LOCK_GUARD(_mtx_sockFd);
        if (!_sockFd) {
            //防止多次触发onErr事件
            return false;
        }
    }

    closeSock();

    weak_ptr<Socket> weakSelf = shared_from_this();
    _poller->async([weakSelf,err]() {
        auto strongSelf=weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        LOCK_GUARD(strongSelf->_mtx_event);
        strongSelf->_errCB(err);
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
        LOCK_GUARD(_mtx_sockFd);
        sock = _sockFd;
    }

    if (!sock) {
        //如果已断开连接或者发送超时
        return -1;
    }

    {
        LOCK_GUARD(_mtx_bufferWaiting);
        _bufferWaiting.emplace_back(sock->type() == SockNum::Sock_UDP ? std::make_shared<BufferSock>(buf,addr,addr_len) : buf);
    }

    if(try_flush){
        if (_canSendSock) {
            //该socket可写
            return flushData(sock, false) ? size : -1;
        }

        //该socket不可写,判断发送超时
        if (_lastFlushTicker.elapsedTime() > _sendTimeOutMS) {
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
        flag = _flushCB();
    }
    if (!flag) {
        setOnFlush(nullptr);
    }
}

void Socket::closeSock() {
    _conTimer.reset();
    _asyncConnectCB.reset();
    LOCK_GUARD(_mtx_sockFd);
    _sockFd.reset();
}

int Socket::getSendBufferCount(){
    int ret = 0;
    {
        LOCK_GUARD(_mtx_bufferWaiting);
        ret += _bufferWaiting.size();
    }

    {
        LOCK_GUARD(_mtx_bufferSending);
        _bufferSending.for_each([&](BufferList::Ptr &buf){
            ret += buf->count();
        });
    }
    return ret;
}

uint64_t Socket::elapsedTimeAfterFlushed(){
    return _lastFlushTicker.elapsedTime();
}

bool Socket::listen(const SockFD::Ptr &pSock){
    closeSock();
    weak_ptr<SockFD> weakSock = pSock;
    weak_ptr<Socket> weakSelf = shared_from_this();
    _enableRecv = true;
    int result = _poller->addEvent(pSock->rawFd(), Event_Read | Event_Error, [weakSelf,weakSock](int event) {
        auto strongSelf = weakSelf.lock();
        auto strongSock = weakSock.lock();
        if(!strongSelf || !strongSock) {
            return;
        }
        strongSelf->onAccept(strongSock,event);
    });

    if(result == -1){
        WarnL << "开始Poll监听失败";
        return false;
    }
    LOCK_GUARD(_mtx_sockFd);
    _sockFd = pSock;
    return true;
}
bool Socket::listen(const uint16_t port, const char* localIp, int backLog) {
    int sock = SockUtil::listen(port, localIp, backLog);
    if (sock == -1) {
        return false;
    }
    return listen(makeSock(sock,SockNum::Sock_TCP));
}
bool Socket::bindUdpSock(const uint16_t port, const char* localIp) {
    closeSock();
    int sock = SockUtil::bindUdpSock(port, localIp);
    if (sock == -1) {
        return false;
    }
    auto pSock = makeSock(sock,SockNum::Sock_UDP);
    if(!attachEvent(pSock,true)){
        WarnL << "开始Poll监听失败";
        return false;
    }
    LOCK_GUARD(_mtx_sockFd);
    _sockFd = pSock;
    return true;
}
int Socket::onAccept(const SockFD::Ptr &pSock,int event) {
    int peerfd;
    while (true) {
        if (event & Event_Read) {
            do{
                peerfd = accept(pSock->rawFd(), NULL, NULL);
            }while(-1 == peerfd && UV_EINTR == get_uv_error(true));


            if (peerfd == -1) {
                int err = get_uv_error(true);
                if (err == UV_EAGAIN) {
                    //没有新连接
                    return 0;
                }
                ErrorL << "tcp服务器监听异常:" << uv_strerror(err);
                onError(pSock);
                return -1;
            }
            SockUtil::setNoSigpipe(peerfd);
            SockUtil::setNoBlocked(peerfd);
            SockUtil::setNoDelay(peerfd);
            SockUtil::setSendBuf(peerfd);
            SockUtil::setRecvBuf(peerfd);
            SockUtil::setCloseWait(peerfd);

            //拦截默认的Socket构造行为，
            //在TcpServer中，默认的行为是子Socket的网络事件会派发到其他poll线程
            //这样就可以发挥最大的网络性能
            Socket::Ptr peerSock ;

            {
                LOCK_GUARD(_mtx_event);
                peerSock = _beforeAcceptCB(_poller);
            }

            if(!peerSock){
                //此处是默认构造行为，也就是子Socket
                //共用父Socket的poll线程以及事件执行线程
                peerSock = std::make_shared<Socket>(_poller);
            }

            //设置好fd,以备在TcpSession的构造函数中可以正常访问该fd
            auto sockFD = peerSock->setPeerSock(peerfd);

            //在accept事件中，TcpServer对象会创建TcpSession对象并绑定该Socket的相关事件(onRead/onErr)
            //所以在这之前千万不能就把peerfd加入poll监听
            {
                LOCK_GUARD(_mtx_event);
                _acceptCB(peerSock);
            }
            //把该peerfd加入poll监听，这个时候可能会触发其数据接收事件
            if(!peerSock->attachEvent(sockFD, false)){
                //加入poll监听失败，我们通知TcpServer该Socket无效
                peerSock->emitErr(SockException(Err_eof,"attachEvent failed"));
            }
        }

        if (event & Event_Error) {
            ErrorL << "tcp服务器监听异常:" << get_uv_errmsg();
            onError(pSock);
            return -1;
        }
    }
}

SockFD::Ptr Socket::setPeerSock(int sock) {
    closeSock();
    auto pSock = makeSock(sock,SockNum::Sock_TCP);
    LOCK_GUARD(_mtx_sockFd);
    _sockFd = pSock;
    return pSock;
}

string Socket::get_local_ip() {
    LOCK_GUARD(_mtx_sockFd);
    if (!_sockFd) {
        return "";
    }
    return SockUtil::get_local_ip(_sockFd->rawFd());
}

uint16_t Socket::get_local_port() {
    LOCK_GUARD(_mtx_sockFd);
    if (!_sockFd) {
        return 0;
    }
    return SockUtil::get_local_port(_sockFd->rawFd());

}

string Socket::get_peer_ip() {
    LOCK_GUARD(_mtx_sockFd);
    if (!_sockFd) {
        return "";
    }
    return SockUtil::get_peer_ip(_sockFd->rawFd());
}

uint16_t Socket::get_peer_port() {
    LOCK_GUARD(_mtx_sockFd);
    if (!_sockFd) {
        return 0;
    }
    return SockUtil::get_peer_port(_sockFd->rawFd());
}

string Socket::getIdentifier() const{
    static string class_name = "Socket:";
    return class_name + to_string(reinterpret_cast<uint64_t>(this));
}

bool Socket::flushData(const SockFD::Ptr &pSock,bool bPollerThread) {
    decltype(_bufferSending) bufferSendingTmp;
    {
        LOCK_GUARD(_mtx_bufferSending);
        if(!_bufferSending.empty()){
            bufferSendingTmp.swap(_bufferSending);
        }
    }

    if (bufferSendingTmp.empty()) {
        _lastFlushTicker.resetTime();
        do{
            {
                //_bufferSending列队中数据为空，那么我们接着消费_bufferWaiting列队中的数据
                LOCK_GUARD(_mtx_bufferWaiting);
                if (!_bufferWaiting.empty()) {
                    //把_bufferWaiting列队数据放置到_bufferSending列队
                    bufferSendingTmp.emplace_back(std::make_shared<BufferList>(_bufferWaiting));
                    break;
                }
            }
            //如果_bufferWaiting列队中数据也为空,那么说明消费完所有未发送缓存数据
            if (bPollerThread) {
                //主线程触发该函数，那么该socket应该已经加入了可写事件的监听；
                //那么在数据列队清空的情况下，我们需要关闭监听以免触发无意义的事件回调
                stopWriteAbleEvent(pSock);
                onFlushed(pSock);
            }
            return true;
        }while(0);
    }

    int sockFd = pSock->rawFd();
    bool isUdp = pSock->type() == SockNum::Sock_UDP;
    while (!bufferSendingTmp.empty()) {
        auto &packet = bufferSendingTmp.front();
        int n = packet->send(sockFd,_sock_flags,isUdp);
        if(n > 0){
            //全部或部分发送成功
            if(packet->empty()){
                //全部发送成功
                bufferSendingTmp.pop_front();
                continue;
            }
            //部分发送成功
            if (!bPollerThread) {
                //如果该函数是主线程触发的，那么该socket应该已经加入了可写事件的监听，所以我们不需要再次加入监听
                startWriteAbleEvent(pSock);
            }
            break;
        }

        //一个都没发送成功
        int err = get_uv_error(true);
        if (err == UV_EAGAIN) {
            //等待下一次发送
            if(!bPollerThread){
                //如果该函数是主线程触发的，那么该socket应该已经加入了可写事件的监听，所以我们不需要再次加入监听
                startWriteAbleEvent(pSock);
            }
            break;
        }
        //其他错误代码，发生异常
        onError(pSock);
        return false;
    }

    //回滚未发送完毕的数据
    if(!bufferSendingTmp.empty()){
        //有剩余数据
        LOCK_GUARD(_mtx_bufferSending);
        bufferSendingTmp.swap(_bufferSending);
        _bufferSending.append(bufferSendingTmp);
        //bufferSendingTmp未全部发送完毕，说明该socket不可写，直接返回
        return true;
    }

    //bufferSendingTmp已经全部发送完毕，说明该socket还可写，我们尝试继续写
    //如果是poller线程，我们尝试再次写一次(因为可能其他线程调用了send函数又有新数据了)
    //如果是非poller线程，那么本次操作本来就是send函数触发的，所以不可能还有新的数据(我们假定send函数只有其他单独一个线程调用)
    return bPollerThread ? flushData(pSock,bPollerThread) : true;
}

void Socket::onWriteAble(const SockFD::Ptr &pSock) {
    bool emptyWaiting;
    bool emptySending;
    {
        LOCK_GUARD(_mtx_bufferWaiting);
        emptyWaiting = _bufferWaiting.empty();
    }

    {
        LOCK_GUARD(_mtx_bufferSending);
        emptySending = _bufferSending.empty();
    }

    if(emptyWaiting && emptySending){
        //数据已经清空了，我们停止监听可写事件
        stopWriteAbleEvent(pSock);
    }else {
        //我们尽量让其他线程来发送数据，不要占用主线程太多性能
        //WarnL << "主线程发送数据";
        flushData(pSock, true);
    }
}


void Socket::startWriteAbleEvent(const SockFD::Ptr &pSock) {
    //ErrorL;
    _canSendSock = false;
    int flag = _enableRecv ? Event_Read : 0;
    _poller->modifyEvent(pSock->rawFd(), flag | Event_Error | Event_Write);
}

void Socket::stopWriteAbleEvent(const SockFD::Ptr &pSock) {
    //ErrorL;
    _canSendSock = true;
    int flag = _enableRecv ? Event_Read : 0;
    _poller->modifyEvent(pSock->rawFd(), flag | Event_Error);
}
void Socket::enableRecv(bool enabled) {
    if(_enableRecv == enabled){
        return;
    }
    _enableRecv = enabled;
    int flag = _enableRecv ? Event_Read : 0;
    _poller->modifyEvent(rawFD(), flag | Event_Error | Event_Write);
}
SockFD::Ptr Socket::makeSock(int sock,SockNum::SockType type){
    return std::make_shared<SockFD>(sock,type,_poller);
}

int Socket::rawFD() const{
    LOCK_GUARD(_mtx_sockFd);
    if(!_sockFd){
        return -1;
    }
    return _sockFd->rawFd();
}

void Socket::setSendTimeOutSecond(uint32_t second){
    _sendTimeOutMS = second * 1000;
}

BufferRaw::Ptr Socket::obtainBuffer() {
    return std::make_shared<BufferRaw>();//_bufferPool.obtain();
}

bool Socket::isSocketBusy() const{
    return !_canSendSock.load();
}

const EventPoller::Ptr &Socket::getPoller() const{
    return _poller;
}

bool Socket::cloneFromListenSocket(const Socket &other){
    SockFD::Ptr fd;
    {
        LOCK_GUARD(other._mtx_sockFd);
        if(!other._sockFd){
            WarnL << "sockfd of src socket is null!";
            return false;
        }
        fd = std::make_shared<SockFD>(*(other._sockFd), _poller);
    }
    return listen(fd);
}

bool Socket::setSendPeerAddr(const struct sockaddr *peerAddr) {
    LOCK_GUARD(_mtx_sockFd);
    if(!_sockFd){
        return false;
    }
    if(_sockFd->type() != SockNum::Sock_UDP){
        return false;
    }
    return 0 == ::connect(_sockFd->rawFd(),peerAddr, sizeof(struct sockaddr));
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
    buffer->assign(buf,size);
    return send(buffer);
}

///////////////SocketHelper///////////////////

SocketHelper::SocketHelper(const Socket::Ptr &sock) {
    setSock(sock);
}

SocketHelper::~SocketHelper() {}

void SocketHelper::setSock(const Socket::Ptr &sock) {
    _sock = sock;
    if(_sock){
        _poller = _sock->getPoller();
    }
}

EventPoller::Ptr SocketHelper::getPoller(){
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
    }else{
        buffer = _sock->obtainBuffer();
    }
    if(data && len){
        buffer->assign((const char *)data,len);
    }
    return buffer;
};

void SocketHelper::shutdown(const SockException &ex) {
    if (_sock) {
        _sock->emitErr(ex);
    }
}

string SocketHelper::get_local_ip(){
    if(_sock && _local_ip.empty()){
        _local_ip = _sock->get_local_ip();
    }
    return _local_ip;
}

uint16_t SocketHelper::get_local_port() {
    if(_sock && _local_port == 0){
        _local_port = _sock->get_local_port();
    }
    return _local_port;
}

string SocketHelper::get_peer_ip(){
    if(_sock && _peer_ip.empty()){
        _peer_ip = _sock->get_peer_ip();
    }
    return _peer_ip;
}

uint16_t SocketHelper::get_peer_port() {
    if(_sock && _peer_port == 0){
        _peer_port = _sock->get_peer_port();
    }
    return _peer_port;
}

bool SocketHelper::isSocketBusy() const{
    if (!_sock) {
        return true;
    }
    return _sock->isSocketBusy();
}

Task::Ptr SocketHelper::async(TaskIn &&task, bool may_sync) {
    return _poller->async(std::move(task),may_sync);
}

Task::Ptr SocketHelper::async_first(TaskIn &&task, bool may_sync) {
    return _poller->async_first(std::move(task),may_sync);
}

void SocketHelper::setSendFlushFlag(bool try_flush){
    _try_flush = try_flush;
}

void SocketHelper::setSendFlags(int flags){
    if(!_sock){
        return;
    }
    _sock->setSendFlags(flags);
}

}  // namespace toolkit



