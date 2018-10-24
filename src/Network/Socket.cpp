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

namespace toolkit {

Socket::Socket(const EventPoller::Ptr &poller,
			   const TaskExecutor::Ptr &executor) {
	_poller = poller;
	if(!_poller){
		_poller = EventPollerPool::Instance().getPoller();
	}
	_executor = executor;
	if(!_executor){
		_executor = _poller;
	}

    _canSendSock = true;
	_readCB = [](const Buffer::Ptr &buf,struct sockaddr *) {
		WarnL << "Socket not set readCB";
	};
	_errCB = [](const SockException &err) {
		WarnL << "Socket not set errCB:" << err.what();
	};
	_acceptCB = [](Socket::Ptr &sock) {
		WarnL << "Socket not set acceptCB";
	};
	_flushCB = []() {return true;};

	_beforeAcceptCB = [](const EventPoller::Ptr &poller,const TaskExecutor::Ptr &executor){
		return nullptr;
	};
}
Socket::~Socket() {
	closeSock();
	//TraceL << endl;
}

void Socket::setOnRead(const onReadCB &cb) {
	if (cb) {
		_readCB = cb;
	} else {
		_readCB = [](const Buffer::Ptr &buf,struct sockaddr *) {
			WarnL << "Socket not set readCB";
		};
	}
}
void Socket::setOnErr(const onErrCB &cb) {
	if (cb) {
		_errCB = cb;
	} else {
		_errCB = [](const SockException &err) {
			WarnL << "Socket not set errCB";
		};
	}
}
void Socket::setOnAccept(const onAcceptCB &cb) {
	if (cb) {
		_acceptCB = cb;
	} else {
		_acceptCB = [](Socket::Ptr &sock) {
			WarnL << "Socket not set acceptCB";
		};
	}
}
void Socket::setOnFlush(const onFlush &cb) {
	if (cb) {
		_flushCB = cb;
	} else {
		_flushCB = []() {return true;};
	}
}

//设置Socket生成拦截器
void Socket::setOnBeforeAccept(const onBeforeAcceptCB &cb){
	if (cb) {
		_beforeAcceptCB = cb;
	} else {
		_beforeAcceptCB = [](const EventPoller::Ptr &poller,const TaskExecutor::Ptr &executor) {
			return nullptr;
		};
	}
}
void Socket::connect(const string &url, uint16_t port,const onErrCB &connectCB, float timeoutSec,const char *localIp,uint16_t localPort) {
	closeSock();
	int sock = SockUtil::connect(url.data(), port, true, localIp, localPort);
	if (sock < 0) {
		connectCB(SockException(Err_other, get_uv_errmsg(true)));
		return;
	}
	auto sockFD = makeSock(sock);
	weak_ptr<Socket> weakSelf = shared_from_this();
	weak_ptr<SockFD> weakSock = sockFD;
	shared_ptr<bool> bTriggered = std::make_shared<bool>(false);//回调被触发过

	int result;
	if(_poller == _executor){
		result = _poller->addEvent(sock, Event_Write, [weakSelf,weakSock,connectCB,bTriggered](int event) {
			auto strongSelf = weakSelf.lock();
			auto strongSock = weakSock.lock();
			if(!strongSelf || !strongSock ||  *bTriggered) {
				return;
			}
			*bTriggered = true;
			strongSelf->onConnected(strongSock,connectCB);
		});
	}else{
		result = _poller->addEvent(sock, Event_Write, [weakSelf,weakSock,connectCB,bTriggered](int event) {
			auto strongSelf = weakSelf.lock();
			if(!strongSelf || *bTriggered) {
				return;
			}
			*bTriggered = true;
			strongSelf->_executor->async([weakSelf,weakSock,connectCB](){
				auto strongSelf = weakSelf.lock();
				auto strongSock = weakSock.lock();
				if(!strongSelf || !strongSock) {
					return;
				}
				strongSelf->onConnected(strongSock,connectCB);
			});
		});
	}

	if(result == -1){
		WarnL << "开始Poll监听失败";
		SockException err(Err_other,"开始Poll监听失败");
		connectCB(err);
		return;
	}

	_conTimer = std::make_shared<Timer>(timeoutSec, [weakSelf,weakSock,connectCB,bTriggered]() {
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
	},_executor);
	lock_guard<mutex> lck(_mtx_sockFd);
	_sockFd = sockFD;
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

	int result;
	if(_poller == _executor){
		result = _poller->addEvent(pSock->rawFd(), Event_Read | Event_Error | Event_Write, [weakSelf,weakSock,isUdp](int event) {
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
	}else{
		result = _poller->addEvent(pSock->rawFd(), Event_Read | Event_Error | Event_Write, [weakSelf,weakSock,isUdp](int event) {
			auto strongSelf = weakSelf.lock();
			if(!strongSelf) {
				return;
			}
			strongSelf->_executor->async([weakSelf,weakSock,isUdp,event](){
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
		});
	}

	return -1 != result;
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
		if (nread < 127) {
			nread = 127;
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

        _readCB(buf, &peerAddr);
	}

    return 0;
}
void Socket::onError(const SockFD::Ptr &pSock) {
	emitErr(getSockErr(pSock));
}
bool Socket::emitErr(const SockException& err,bool close ,bool maySync) {
	{
		lock_guard<mutex> lck(_mtx_sockFd);
		if (!_sockFd) {
            //防止多次触发onErr事件
			return false;
		}
	}

	weak_ptr<Socket> weakSelf = shared_from_this();
	_executor->async([weakSelf,err]() {
		auto strongSelf=weakSelf.lock();
		if (!strongSelf) {
			return;
		}
		strongSelf->_errCB(err);
	},maySync);

    if(close){
        closeSock();
    }
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
    BufferString::Ptr ptr = std::make_shared<BufferString>(buf);
    return send(ptr,flags,peerAddr);
}

int Socket::send(string &&buf, int flags,struct sockaddr* peerAddr) {
    BufferString::Ptr ptr = std::make_shared<BufferString>(std::move(buf));
    return send(ptr,flags,peerAddr);
}
    
uint32_t Socket::getBufSecondLength(){
    if(_sendPktBuf.empty()){
        return 0;
    }
    return _sendPktBuf.back()->getStamp() - _sendPktBuf.front()->getStamp() ;
}
    
int Socket::send(const Buffer::Ptr &buf, int flags ,struct sockaddr *peerAddr){
	if(!buf || !buf->size()){
		return 0;
	}
	SockFD::Ptr sock;
	{
		lock_guard<mutex> lck(_mtx_sockFd);
		sock = _sockFd;
	}
	if (!sock ) {
        //如果已断开连接或者发送超时
		return -1;
	}

    int ret = buf->size();

	Packet::Ptr packet = std::make_shared<Packet>();
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
        flag = _flushCB();
    }
	if (!flag) {
		setOnFlush(nullptr);
	}
}

void Socket::closeSock() {
	_conTimer.reset();
	lock_guard<mutex> lck(_mtx_sockFd);
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
	int result;
	if(_poller == _executor){
		result = _poller->addEvent(sock, Event_Read | Event_Error, [weakSelf,weakSock](int event) {
			auto strongSelf = weakSelf.lock();
			auto strongSock = weakSock.lock();
			if(!strongSelf || !strongSock) {
				return;
			}
			strongSelf->onAccept(strongSock,event);
		});
	} else{
		result = _poller->addEvent(sock, Event_Read | Event_Error, [weakSelf,weakSock](int event) {
			auto strongSelf = weakSelf.lock();
			if(!strongSelf) {
				return;
			}
			strongSelf->_executor->async([weakSelf,weakSock,event](){
				auto strongSelf = weakSelf.lock();
				auto strongSock = weakSock.lock();
				if(!strongSelf || !strongSock) {
					return;
				}
				strongSelf->onAccept(strongSock,event);
			});
		});
	}

	if(result == -1){
		WarnL << "开始Poll监听失败";
		return false;
	}
	lock_guard<mutex> lck(_mtx_sockFd);
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
	lock_guard<mutex> lck(_mtx_sockFd);
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


			Socket::Ptr peerSock ;
			{
			    //拦截默认的Socket构造行为，
                //在TcpServer中，默认的行为是子Socket的网络事件会派发到其他poll线程
                //这样就可以发挥最大的网络性能
				peerSock = _beforeAcceptCB(_poller,_executor);
			}

			if(!peerSock){
			    //此处是默认构造行为，也就是子Socket
                //共用父Socket的poll线程以及事件执行线程
				peerSock = std::make_shared<Socket>(_poller,_executor);
			}

            //设置好fd,以备在TcpSession的构造函数中可以正常访问该fd
            auto sockFD = peerSock->setPeerSock(peerfd);

            {
				//在accept事件中，TcpServer对象会创建TcpSession对象并绑定该Socket的相关事件(onRead/onErr)
				//所以在这之前千万不能就把peerfd加入poll监听
				_acceptCB(peerSock);
			}
			//把该peerfd加入poll监听，这个时候可能会触发其数据接收事件
			if(!peerSock->attachEvent(sockFD, false)){
                //加入poll监听失败，我们通知TcpServer该Socket无效
				peerSock->emitErr(SockException(Err_eof,"attachEvent failed"), true);
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
    auto pSock = makeSock(sock);
	lock_guard<mutex> lck(_mtx_sockFd);
	_sockFd = pSock;
	return pSock;
}

string Socket::get_local_ip() {
    lock_guard<mutex> lck(_mtx_sockFd);
	if (!_sockFd) {
		return "";
	}
	return SockUtil::get_local_ip(_sockFd->rawFd());
}

uint16_t Socket::get_local_port() {
    lock_guard<mutex> lck(_mtx_sockFd);
	if (!_sockFd) {
		return 0;
	}
	return SockUtil::get_local_port(_sockFd->rawFd());

}

string Socket::get_peer_ip() {
    lock_guard<mutex> lck(_mtx_sockFd);
	if (!_sockFd) {
		return "";
	}
	return SockUtil::get_peer_ip(_sockFd->rawFd());
}

uint16_t Socket::get_peer_port() {
    lock_guard<mutex> lck(_mtx_sockFd);
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
    if(_sendPktBuf.empty()){
        _sendPktBuf.swap(sendPktBuf_copy);
    }else{
        _sendPktBuf.insert(_sendPktBuf.begin(), sendPktBuf_copy.begin(),sendPktBuf_copy.end());
    }
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
    _poller->modifyEvent(rawFD(), flag | Event_Error | Event_Write);
}
SockFD::Ptr Socket::makeSock(int sock){
    return std::make_shared<SockFD>(sock,_poller);
}

int Socket::rawFD() const{
    lock_guard<mutex> lck(_mtx_sockFd);
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
const TaskExecutor::Ptr &Socket::getExecutor() const{
	return _executor;
}

const EventPoller::Ptr &Socket::getPoller() const{
	return _poller;
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
		_executor = _sock->getExecutor();
    }
}
void SocketHelper::setExecutor(const TaskExecutor::Ptr &excutor){
	if(excutor){
		_executor = excutor;
	}
}

TaskExecutor::Ptr SocketHelper::getExecutor(){
    return _executor;
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

    send(buf);
    return *this;
}

//发送字符串
SocketHelper &SocketHelper::operator<<(const string &buf) {
    if (!_sock) {
        return *this;
    }
    send(buf);
    return *this;
}

//发送字符串
SocketHelper &SocketHelper::operator<<(string &&buf) {
    if (!_sock) {
        return *this;
    }
	send(std::move(buf));
    return *this;
}

//发送Buffer对象
SocketHelper &SocketHelper::operator<<(const Buffer::Ptr &buf) {
    if (!_sock) {
        return *this;
    }
	send(buf);
    return *this;
}


//////////////////send系列函数//////////////////
int SocketHelper::send(const string &buf) {
    if (!_sock) {
        return -1;
    }
    auto buffer = std::make_shared<BufferString>(buf);
    return send(buffer);
}

int SocketHelper::send(string &&buf) {
    if (!_sock) {
        return -1;
    }
	auto buffer = std::make_shared<BufferString>(std::move(buf));
	return send(buffer);
}

int SocketHelper::send(const char *buf, int size) {
    if (!_sock) {
        return -1;
    }
	auto buffer = _sock->obtainBuffer();
	buffer->assign(buf,size);
	return send(buffer);
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

BufferRaw::Ptr SocketHelper::obtainBuffer(const void *data, int len) {
	BufferRaw::Ptr buffer = obtainBuffer();
	if(buffer){
		buffer->assign((const char *)data,len);
	}
	return buffer;
};

//触发onError事件
void SocketHelper::shutdown() {
    if (_sock) {
        _sock->emitErr(SockException(Err_other, "self shutdown"),true,false);
    }
}

/////////获取ip或端口///////////
const string& SocketHelper::get_local_ip(){
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

const string& SocketHelper::get_peer_ip(){
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

bool SocketHelper::async(const TaskExecutor::Task &task, bool may_sync) {
	return _executor->async(task,may_sync);
};
bool SocketHelper::async_first(const TaskExecutor::Task &task, bool may_sync) {
	return _executor->async_first(task,may_sync);
};
bool SocketHelper::sync(const TaskExecutor::Task &task) {
	return _executor->sync(task);
};
bool SocketHelper::sync_first(const TaskExecutor::Task &task) {
	return _executor->sync_first(task);
}

}  // namespace toolkit



