/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <type_traits>
#include "sockutil.h"
#include "Socket.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/uv_errno.h"
#include "Util/TimeTicker.h"
#include "Thread/semaphore.h"
#include "Poller/EventPoller.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Thread;
using namespace ZL::Poller;

namespace ZL {
namespace Network {

Socket::Socket() {
    _canSendSock = true;
	_readCB = [](const Buffer::Ptr &buf,struct sockaddr *) {
		WarnL << "Socket not set readCB";
	};
	_errCB = [](const SockException &err) {
		WarnL << "Socket not set errCB";
	};
	_acceptCB = [](Socket::Ptr &sock) {
		WarnL << "Socket not set acceptCB";
	};
	_flushCB = []() {return true;};
}
Socket::~Socket() {
	closeSock();
	//TraceL << endl;
}

void Socket::setOnRead(const onReadCB &cb) {
	lock_guard<spin_mutex> lck(_mtx_read);
	if (cb) {
		_readCB = cb;
	} else {
		_readCB = [](const Buffer::Ptr &buf,struct sockaddr *) {
			WarnL << "Socket not set readCB";
		};
	}
}
void Socket::setOnErr(const onErrCB &cb) {
	lock_guard<spin_mutex> lck(_mtx_err);
	if (cb) {
		_errCB = cb;
	} else {
		_errCB = [](const SockException &err) {
			WarnL << "Socket not set errCB";
		};
	}
}
void Socket::setOnAccept(const onAcceptCB &cb) {
	lock_guard<spin_mutex> lck(_mtx_accept);
	if (cb) {
		_acceptCB = cb;
	} else {
		_acceptCB = [](Socket::Ptr &sock) {
			WarnL << "Socket not set acceptCB";
		};
	}
}
void Socket::setOnFlush(const onFlush &cb) {
	lock_guard<spin_mutex> lck(_mtx_flush);
	if (cb) {
		_flushCB = cb;
	} else {
		_flushCB = []() {return true;};
	}
}
void Socket::connect(const string &url, uint16_t port,const onErrCB &connectCB, float timeoutSec) {
	closeSock();
	int sock = SockUtil::connect(url.data(), port);
	if (sock < 0) {
		connectCB(SockException(Err_other, get_uv_errmsg(true)));
		return;
	}
	auto sockFD = makeSock(sock);
	weak_ptr<Socket> weakSelf = shared_from_this();
	weak_ptr<SockFD> weakSock = sockFD;
	std::shared_ptr<bool> bTriggered(new bool(false));//回调被触发过
	auto result = EventPoller::Instance().addEvent(sock, Event_Write, [weakSelf,weakSock,connectCB,bTriggered](int event) {
		auto strongSelf = weakSelf.lock();
		auto strongSock = weakSock.lock();
		if(!strongSelf || !strongSock || *bTriggered) {
			return;
		}
		*bTriggered = true;
		strongSelf->onConnected(strongSock,connectCB);
	});
	if(result == -1){
		WarnL << "开始Poll监听失败";
		SockException err(Err_other,"开始Poll监听失败");
		connectCB(err);
		return;
	}

	_conTimer.reset(new Timer(timeoutSec, [weakSelf,weakSock,connectCB,bTriggered]() {
		auto strongSelf = weakSelf.lock();
		auto strongSock = weakSock.lock();
		if(!strongSelf || !strongSock || *bTriggered) {
			return false;
		}
		*bTriggered = true;
		SockException err(Err_timeout, uv_strerror(UV_ETIMEDOUT));
		strongSelf->emitErr(err);
		connectCB(err);
		return false;
	}));
	lock_guard<spin_mutex> lck(_mtx_sockFd);
	_sockFd = sockFD;
}

void Socket::onConnected(const SockFD::Ptr &pSock,const onErrCB &connectCB) {
	_conTimer.reset();
	auto err = getSockErr(pSock, false);
    do {
        if (!err) {
            EventPoller::Instance().delEvent(pSock->rawFd());
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
		return SockException(Err_refused, uv_strerror(error));
	case UV_ETIMEDOUT:
		return SockException(Err_timeout, uv_strerror(error));
	default:
		return SockException(Err_other, uv_strerror(error));
	}
}
bool Socket::attachEvent(const SockFD::Ptr &pSock,bool isUdp) {
	weak_ptr<Socket> weakSelf = shared_from_this();
	weak_ptr<SockFD> weakSock = pSock;
	_enableRecv = true;
	return -1 != EventPoller::Instance().addEvent(pSock->rawFd(),
			Event_Read | Event_Error | Event_Write,
			[weakSelf,weakSock,isUdp](int event) {

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
			strongSelf->onRead(strongSock,!isUdp);
		}
		if (event & Event_Write) {
			strongSelf->onWriteAble(strongSock);
		}
	});
}

int Socket::onRead(const SockFD::Ptr &pSock,bool mayEof) {
    TimeTicker1(1);
	int ret = 0;
	int sock = pSock->rawFd();
	while (_enableRecv) {
#if defined(_WIN32)
		unsigned long nread;
#else
		int nread;
#endif //defined(_WIN32)
		ioctl(sock, FIONREAD, &nread);
		if (nread < 4095) {
			nread = 4095;
		}
		struct sockaddr peerAddr;
		socklen_t len = sizeof(struct sockaddr);
        BufferRaw::Ptr buf = obtainBuffer();
		buf->setCapacity(nread + 1);
		do {
			nread = recvfrom(sock, buf->data(), nread, 0, &peerAddr, &len);
		} while (-1 == nread && UV_EINTR == get_uv_error(true));

		if (nread == 0) {
			if (mayEof) {
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
		buf->data()[nread] = '\0';
		buf->setSize(nread);

        lock_guard<spin_mutex> lck(_mtx_read);
        _readCB(buf, &peerAddr);
	}
    return 0;
}
void Socket::onError(const SockFD::Ptr &pSock) {
	emitErr(getSockErr(pSock));
}
bool Socket::emitErr(const SockException& err) {
	{
		lock_guard<spin_mutex> lck(_mtx_sockFd);
		if (!_sockFd) {
            //防止多次触发onErr事件
			return false;
		}
	}
	weak_ptr<Socket> weakSelf = shared_from_this();
	ASYNC_TRACE([weakSelf,err]() {
		auto strongSelf=weakSelf.lock();
		if (!strongSelf) {
			return;
		}

        lock_guard<spin_mutex> lck(strongSelf->_mtx_err);
        strongSelf->_errCB(err);
	});
	closeSock();
	return true;
}


int Socket::send(const char* buf, int size, int flags,struct sockaddr* peerAddr) {
    if (size <= 0) {
        size = strlen(buf);
        if (!size) {
            return 0;
        }
    }
    BufferRaw::Ptr ptr = obtainBuffer();
    ptr->assign(buf,size);
    return send(ptr,flags,peerAddr);
}
int Socket::send(const string &buf, int flags, struct sockaddr* peerAddr) {
    BufferString::Ptr ptr(new BufferString(buf));
    return send(ptr,flags,peerAddr);
}

int Socket::send(string &&buf, int flags,struct sockaddr* peerAddr) {
    BufferString::Ptr ptr(new BufferString(std::move(buf)));
    return send(ptr,flags,peerAddr);
}
    
uint32_t Socket::getBufSecondLength(){
    if(_sendPktBuf.empty()){
        return 0;
    }
    return _sendPktBuf.back()->getStamp() - _sendPktBuf.front()->getStamp() ;
}
    
int Socket::send(const Buffer::Ptr &buf, int flags ,struct sockaddr *peerAddr){
	if(!buf->size()){
		return 0;
	}
	SockFD::Ptr sock;
	{
		lock_guard<spin_mutex> lck(_mtx_sockFd);
		sock = _sockFd;
	}
	if (!sock ) {
        //如果已断开连接或者发送超时
		return -1;
	}

    int ret = buf->size();

	Packet::Ptr packet(new Packet);
    packet->updateStamp();
    packet->setData(buf);
    packet->setFlag(flags);
    packet->setAddr(peerAddr);
	do{//减小临界区
		lock_guard<recursive_mutex> lck(_mtx_sendBuf);
        int bufSec = getBufSecondLength();
		if (bufSec >= _sendBufSec) {
            if(sendTimeout()){
                //一定时间内没有任何数据发送成功，则主动断开socket
                return -1;
            }
            //缓存达到最大限制，未写入任何数据，交给上层应用处理（已经去除主动丢包的特性）
            WarnL << "socket send buffer overflow:" << bufSec << " " << get_peer_ip() << " " << get_peer_port();
            ret = 0;
            break;
		}
		_sendPktBuf.emplace_back(packet);
	}while(0);

	if(_canSendSock){
		//该socket可写
		//WarnL << "后台线程发送数据";
		if(!sendData(sock,false)){
            //发生错误
            return -1;
        }
	}
	return ret;
}


void Socket::onFlushed(const SockFD::Ptr &pSock) {
    bool flag;
    {
        lock_guard<spin_mutex> lck(_mtx_flush);
        flag = _flushCB();
    }
	if (!flag) {
		setOnFlush(nullptr);
	}
}

void Socket::closeSock() {
	_conTimer.reset();
	lock_guard<spin_mutex> lck(_mtx_sockFd);
	_sockFd.reset();
}

bool Socket::listen(const uint16_t port, const char* localIp, int backLog) {
	closeSock();
	int sock = SockUtil::listen(port, localIp, backLog);
	if (sock == -1) {
		return false;
	}
	auto pSock = makeSock(sock);
	weak_ptr<SockFD> weakSock = pSock;
	weak_ptr<Socket> weakSelf = shared_from_this();
	_enableRecv = true;
	auto result = EventPoller::Instance().addEvent(sock, Event_Read | Event_Error, [weakSelf,weakSock](int event) {
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
	lock_guard<spin_mutex> lck(_mtx_sockFd);
	_sockFd = pSock;
	return true;
}
bool Socket::bindUdpSock(const uint16_t port, const char* localIp) {
	closeSock();
	int sock = SockUtil::bindUdpSock(port, localIp);
	if (sock == -1) {
		return false;
	}
	auto pSock = makeSock(sock);
	if(!attachEvent(pSock,true)){
		WarnL << "开始Poll监听失败";
		return false;
	}
	lock_guard<spin_mutex> lck(_mtx_sockFd);
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

			Socket::Ptr peerSock(new Socket());
			if(peerSock->setPeerSock(peerfd)){
                lock_guard<spin_mutex> lck(_mtx_accept);
                _acceptCB(peerSock);
			}
		}

		if (event & Event_Error) {
			ErrorL << "tcp服务器监听异常:" << get_uv_errmsg();
			onError(pSock);
			return -1;
		}
	}
}
bool Socket::setPeerSock(int sock) {
	closeSock();
	auto pSock = makeSock(sock);
	if(!attachEvent(pSock,false)){
		WarnL << "开始Poll监听失败";
		return false;
	}
	lock_guard<spin_mutex> lck(_mtx_sockFd);
	_sockFd = pSock;
	return true;
}

string Socket::get_local_ip() {
    lock_guard<spin_mutex> lck(_mtx_sockFd);
	if (!_sockFd) {
		return "";
	}
	return SockUtil::get_local_ip(_sockFd->rawFd());
}

uint16_t Socket::get_local_port() {
    lock_guard<spin_mutex> lck(_mtx_sockFd);
	if (!_sockFd) {
		return 0;
	}
	return SockUtil::get_local_port(_sockFd->rawFd());

}

string Socket::get_peer_ip() {
    lock_guard<spin_mutex> lck(_mtx_sockFd);
	if (!_sockFd) {
		return "";
	}
	return SockUtil::get_peer_ip(_sockFd->rawFd());
}

uint16_t Socket::get_peer_port() {
    lock_guard<spin_mutex> lck(_mtx_sockFd);
	if (!_sockFd) {
		return 0;
	}
	return SockUtil::get_peer_port(_sockFd->rawFd());
}

bool Socket::sendData(const SockFD::Ptr &pSock, bool bMainThread){
    decltype(_sendPktBuf) sendPktBuf_copy;
    {
        lock_guard<recursive_mutex> lck(_mtx_sendBuf);
        auto sz = _sendPktBuf.size();
        if (!sz) {
            if (bMainThread) {
                //主线程触发该函数，那么该socket应该已经加入了可写事件的监听；
                //那么在数据列队清空的情况下，我们需要关闭监听以免触发无意义的事件回调
                stopWriteAbleEvent(pSock);
                onFlushed(pSock);
            }
            return true;
        }
        sendPktBuf_copy.swap(_sendPktBuf);
    }
    int sockFd = pSock->rawFd();
    while (!sendPktBuf_copy.empty()) {
        auto &packet = sendPktBuf_copy.front();
        int n = packet->send(sockFd);
        if(n > 0){
            //全部或部分发送成功
            _flushTicker.resetTime();
            if(packet->empty()){
                //全部发送成功
                sendPktBuf_copy.pop_front();
                continue;
            }
            //部分发送成功
            if (!bMainThread) {
                //如果该函数是主线程触发的，那么该socket应该已经加入了可写事件的监听，所以我们不需要再次加入监听
                startWriteAbleEvent(pSock);
            }
            break;
        }

        //一个都没发送成功
        int err = get_uv_error(true);
        if (err == UV_EAGAIN) {
            //等待下一次发送
            if(!bMainThread){
                //如果该函数是主线程触发的，那么该socket应该已经加入了可写事件的监听，所以我们不需要再次加入监听
                startWriteAbleEvent(pSock);
            }
            break;
        }
        //其他错误代码，发生异常
        onError(pSock);
        return false;
    }

    if (sendPktBuf_copy.empty()) {
        //确保真正全部发送完毕
        return sendData(pSock,bMainThread);
    }
    //未发送完毕则回滚数据
    lock_guard<recursive_mutex> lck(_mtx_sendBuf);
    _sendPktBuf.insert(_sendPktBuf.begin(), sendPktBuf_copy.begin(),sendPktBuf_copy.end());
    return true;
}

void Socket::onWriteAble(const SockFD::Ptr &pSock) {
    bool empty;
    {
        lock_guard<recursive_mutex> lck(_mtx_sendBuf);
        empty = _sendPktBuf.empty();
    }
    if(empty){
        //数据已经清空了，我们停止监听可写事件
        stopWriteAbleEvent(pSock);
    }else {
        //我们尽量让其他线程来发送数据，不要占用主线程太多性能
        //WarnL << "主线程发送数据";
        sendData(pSock, true);
	}
}


void Socket::startWriteAbleEvent(const SockFD::Ptr &pSock) {
    //FatalL;
    _canSendSock = false;
    int flag = _enableRecv ? Event_Read : 0;
	EventPoller::Instance().modifyEvent(pSock->rawFd(), flag | Event_Error | Event_Write);
}

void Socket::stopWriteAbleEvent(const SockFD::Ptr &pSock) {
    //FatalL;
    _canSendSock = true;
    int flag = _enableRecv ? Event_Read : 0;
	EventPoller::Instance().modifyEvent(pSock->rawFd(), flag | Event_Error);
}
bool Socket::sendTimeout() {
	if (_flushTicker.elapsedTime() > _sendTimeOutSec * 1000) {
		emitErr(SockException(Err_other, "Socket send timeout"));
		_sendPktBuf.clear();
		return true;
	}
	return false;
}
void Socket::enableRecv(bool enabled) {
    if(_enableRecv == enabled){
        return;
    }
    _enableRecv = enabled;
    int flag = _enableRecv ? Event_Read : 0;
    EventPoller::Instance().modifyEvent(rawFD(), flag | Event_Error | Event_Write);
}
SockFD::Ptr Socket::makeSock(int sock){
    return std::make_shared<SockFD>(sock);
}

int Socket::rawFD() const{
    lock_guard<spin_mutex> lck(_mtx_sockFd);
    if(!_sockFd){
        return -1;
    }
    return _sockFd->rawFd();
}


void Socket::setSendBufSecond(uint32_t second){
    _sendBufSec = second;
}
void Socket::setSendTimeOutSecond(uint32_t second){
    _sendTimeOutSec = second;
}

BufferRaw::Ptr Socket::obtainBuffer() {
    return std::make_shared<BufferRaw>() ;//_bufferPool.obtain();
}

bool Socket::isSocketBusy() const{
    return !_canSendSock.load();
}
///////////////Packet/////////////////////
void Packet::updateStamp(){
    _stamp = (uint32_t)time(NULL);
}
uint32_t Packet::getStamp() const{
    return _stamp;
}
void Packet::setAddr(const struct sockaddr *addr){
    if (addr) {
        if (_addr) {
            *_addr = *addr;
        } else {
            _addr = new struct sockaddr(*addr);
        }
    } else {
        if (_addr) {
            delete _addr;
            _addr = nullptr;
        }
    }
}
int Packet::send(int fd){
	int n;
	do {
		if(_addr){
			n = ::sendto(fd, _data->data() + _offset, _data->size() - _offset, _flag, _addr, sizeof(struct sockaddr));
		}else{
			n = ::send(fd, _data->data() + _offset, _data->size() - _offset, _flag);
		}
	} while (-1 == n && UV_EINTR == get_uv_error(true));

	if(n >= (int)_data->size() - _offset){
		//全部发送成功
		_offset = _data->size();
		_data.reset();
	}else if(n > 0) {
		//部分发送成功
		_offset += n;
	}
	return n;
}
///////////////SocketHelper///////////////////
SocketHelper::SocketHelper(const Socket::Ptr &sock) {
    setSock(sock);
}

SocketHelper::~SocketHelper() {}

//重新设置socket
void SocketHelper::setSock(const Socket::Ptr &sock) {
    _sock = sock;
    if(_sock){
        _local_ip = _sock->get_local_ip();
        _local_port = _sock->get_local_port();
        _peer_ip = _sock->get_peer_ip();
        _peer_port = _sock->get_peer_port();
    }
}

//设置socket flags
SocketHelper &SocketHelper::operator<<(const SocketFlags &flags) {
    _flags = flags._flags;
    return *this;
}

//////////////////operator << 系列函数//////////////////
//发送char *
SocketHelper &SocketHelper::operator<<(const char *buf) {
    if (!_sock) {
        return *this;
    }
    _sock->send(buf, 0, _flags);
    return *this;
}

//发送字符串
SocketHelper &SocketHelper::operator<<(const string &buf) {
    if (!_sock) {
        return *this;
    }
    _sock->send(buf, _flags);
    return *this;
}

//发送字符串
SocketHelper &SocketHelper::operator<<(string &&buf) {
    if (!_sock) {
        return *this;
    }
    _sock->send(std::move(buf), _flags);
    return *this;
}

//发送Buffer对象
SocketHelper &SocketHelper::operator<<(const Buffer::Ptr &buf) {
    if (!_sock) {
        return *this;
    }
    _sock->send(buf, _flags);
    return *this;
}


//////////////////send系列函数//////////////////
int SocketHelper::send(const string &buf) {
    if (!_sock) {
        return -1;
    }
    return _sock->send(buf, _flags);
}

int SocketHelper::send(string &&buf) {
    if (!_sock) {
        return -1;
    }
    return _sock->send(std::move(buf), _flags);
}

int SocketHelper::send(const char *buf, int size) {
    if (!_sock) {
        return -1;
    }
    return _sock->send(buf, size, _flags);
}

int SocketHelper::send(const Buffer::Ptr &buf) {
    if (!_sock) {
        return -1;
    }
    return _sock->send(buf, _flags);
}

////////其他方法////////
//从缓存池中获取一片缓存
BufferRaw::Ptr SocketHelper::obtainBuffer() {
    if (!_sock) {
        return nullptr;
    }
    return _sock->obtainBuffer();
}

//触发onError事件
void SocketHelper::shutdown() {
    if (_sock) {
        _sock->emitErr(SockException(Err_other, "self shutdown"));
    }
}

/////////获取ip或端口///////////
const string& SocketHelper::get_local_ip() const{
    return _local_ip;
}

uint16_t SocketHelper::get_local_port() {
    return _local_port;
}

const string& SocketHelper::get_peer_ip() const{
    return _peer_ip;
}

uint16_t SocketHelper::get_peer_port() {
    return _peer_port;
}

bool SocketHelper::isSocketBusy() const{
    if (!_sock) {
        return true;
    }
    return _sock->isSocketBusy();
}
    
}  // namespace Network
}  // namespace ZL



