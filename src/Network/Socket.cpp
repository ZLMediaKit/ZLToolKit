//
//  Socket.cpp
//  xzl
//
//  Created by xzl on 16/4/13.
//

#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include "sockutil.h"
#include "Socket.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Thread/semaphore.h"
#include "Poller/EventPoller.h"

using namespace ZL::Util;
using namespace ZL::Thread;
using namespace ZL::Poller;

namespace ZL {
namespace Network {

Socket::Socket() {
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
	_enableRecv = true;
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
void Socket::connect(const string &url, uint16_t port, onErrCB &&connectCB,
		int timeoutSec) {
	closeSock();
	int sock = SockUtil::connect(url.data(), port);
	if (sock < 0) {
		connectCB(SockException(Err_other, strerror(errno)));
		return;
	}
	auto sockFD = makeSock(sock);
	weak_ptr<Socket> weakSelf = this->shared_from_this();
	weak_ptr<SockFD> weakSock = sockFD;
	auto result = EventPoller::Instance().addEvent(sock, Event_Write, [weakSelf,weakSock,connectCB](int event) {
		auto strongSelf = weakSelf.lock();
		auto strongSock = weakSock.lock();
		if(!strongSelf || !strongSock) {
			return;
		}
		strongSelf->onConnected(strongSock,connectCB);
	});
	if(result == -1){
		WarnL << "开始Poll监听失败";
		SockException err(Err_other,"开始Poll监听失败");
		connectCB(err);
		return;
	}

	_conTimer.reset(new Timer(timeoutSec, [weakSelf,connectCB]() {
		auto strongSelf=weakSelf.lock();
		if (!strongSelf) {
			return false;
		}
		SockException err(Err_timeout,strerror(ETIMEDOUT));
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
	if (!err) {
		EventPoller::Instance().delEvent(pSock->rawFd());
		if(!attachEvent(pSock,false)){
			WarnL << "开始Poll监听失败";
			err.reset(Err_other, "开始Poll监听失败");
			goto failed;
		}
		pSock->setConnected();
		connectCB(err);
		return;
	}
failed:
	emitErr(err);
	connectCB(err);
}

SockException Socket::getSockErr(const SockFD::Ptr &sock, bool tryErrno) {
	int error = -1, len;
	len = sizeof(int);
	getsockopt(sock->rawFd(), SOL_SOCKET, SO_ERROR, &error, (socklen_t *) &len);
	if (error == 0 && tryErrno) {
		error = errno;
	}
	switch (error) {
	case 0:
	case EINPROGRESS:
		return SockException(Err_success, strerror(error));
	case ECONNREFUSED:
		return SockException(Err_refused, strerror(error));
	case ETIMEDOUT:
		return SockException(Err_timeout, strerror(error));
	default:
		return SockException(Err_other, strerror(error));
	}
}
bool Socket::attachEvent(const SockFD::Ptr &pSock,bool isUdp) {
	weak_ptr<Socket> weakSelf = shared_from_this();
	weak_ptr<SockFD> weakSock = pSock;
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
			strongSelf->onWrite(strongSock,true,strongSelf->_lastSendFlags,isUdp);
		}
	});
}

int Socket::onRead(const SockFD::Ptr &pSock,bool mayEof) {
	int ret = 0;
	int sock = pSock->rawFd();
	while (true && _enableRecv.load()) {
		int nread;
		ioctl(sock, FIONREAD, &nread);
		if (nread < 4095) {
			nread = 4095;
		}
		struct sockaddr peerAddr;
		socklen_t len = sizeof(struct sockaddr);
		Buffer::Ptr buf(new Buffer(nread + 1));

		do {
			nread = recvfrom(sock, buf->_data, nread, 0, &peerAddr, &len);
		} while (-1 == nread && EINTR == errno);

		if (nread == 0) {
			if (mayEof) {
				emitErr(SockException(Err_eof, "end of file"));
			}
			return ret;
		}

		if (nread == -1) {
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				emitErr(getSockErr(pSock));
			}
			return ret;
		}
		ret += nread;
		buf->_data[nread] = '\0';
		buf->_size = nread;
		onReadCB cb;
		{
			lock_guard<spin_mutex> lck(_mtx_read);
			cb = _readCB;
		}
		cb(buf, &peerAddr);
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
			return false;
		}
	}
	weak_ptr<Socket> weakSelf = this->shared_from_this();
	ASYNC_TRACE([weakSelf,err]() {
		auto strongSelf=weakSelf.lock();
		if (!strongSelf) {
			return;
		}
		onErrCB cb;
		{
			lock_guard<spin_mutex> lck(strongSelf->_mtx_err);
			cb = strongSelf->_errCB;
		}
		cb(err);
	});
	closeSock();
	return true;
}

int Socket::send(const char *buf, int size,int flags) {
	if (size <= 0) {
		size = strlen(buf);
		if (!size) {
			return 0;
		}
	}
	return send(string(buf,size), flags);
}
int Socket::send(const string &buf, int flags) {
	return realSend(buf,nullptr,flags);
}
int Socket::sendTo(const char* buf, int size, struct sockaddr* peerAddr,int flags) {
	if (size <= 0) {
		size = strlen(buf);
		if (!size) {
			return 0;
		}
	}
	return sendTo(string(buf,size), peerAddr, flags);
}
int Socket::sendTo(const string &buf, struct sockaddr* peerAddr, int flags) {
	return realSend(buf,peerAddr,flags);
}

int Socket::realSend(const string &buf, struct sockaddr *peerAddr,int flags){
	TimeTicker();
	if (buf.empty()) {
		return 0;
	}
	SockFD::Ptr sock;
	{
		lock_guard<spin_mutex> lck(_mtx_sockFd);
		sock = _sockFd;
	}
	if (!sock) {
		return -1;
	}
	bool isUdp = peerAddr;
	std::size_t sz;
	{
		lock_guard<recursive_mutex> lck(_mtx_sendBuf);
		sz = _sendPktBuf.size();
		if (sz >= _iMaxSendPktSize) {
			if (sendTimeout(isUdp)) {
				return -1;
			}
			WarnL << "socket send buffer overflow,previous data has been discarded.";
			sz = 0;
			//the first packet maybe sent partially
			_sendPktBuf.erase(_sendPktBuf.begin()+1,_sendPktBuf.end());
			if(isUdp){
				//udp
				_udpSendPeer.erase(_udpSendPeer.begin()+1,_udpSendPeer.end());
			}
		} else if (sz >= _iMaxSendPktSize / 2) {
			sz = 0;
		}
		_sendPktBuf.emplace_back(buf);
		if(isUdp){
			//udp
			_udpSendPeer.emplace_back(*peerAddr);
		}
	}
	if (sz) {
		return 0;
	}
	_lastSendFlags = flags;
	return onWrite(sock,false,flags,isUdp);
}


void Socket::onFlushed(const SockFD::Ptr &pSock) {
	onFlush cb;
	{
		lock_guard<spin_mutex> lck(_mtx_flush);
		cb = _flushCB;
	}
	if (!cb()) {
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
	weak_ptr<Socket> weakSelf = this->shared_from_this();
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
	struct sockaddr addr;
	socklen_t len = sizeof addr;
	int peerfd;
	while (true) {
		if (event & Event_Read) {
			peerfd = accept(pSock->rawFd(), &addr, &len);
			if (peerfd == -1) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					return 0;
				}
				ErrorL << "tcp服务器监听异常:" << strerror(errno);
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
			if(peerSock->setPeerSock(peerfd, &addr)){
				onAcceptCB cb;
				{
					lock_guard<spin_mutex> lck(_mtx_accept);
					cb = _acceptCB;
				}
				cb(peerSock);
			}
		}

		if (event & Event_Error) {
			ErrorL << "tcp服务器监听异常:" << strerror(errno);
			onError(pSock);
			return -1;
		}
	}
}
bool Socket::setPeerSock(int sock, struct sockaddr *addr) {
	closeSock();
	auto pSock = makeSock(sock);
	if(!attachEvent(pSock,false)){
		WarnL << "开始Poll监听失败";
		return false;
	}
	_peerAddr = *addr;
	lock_guard<spin_mutex> lck(_mtx_sockFd);
	_sockFd = pSock;
	return true;
}

string Socket::get_local_ip() {
	SockFD::Ptr sock;
	{
		lock_guard<spin_mutex> lck(_mtx_sockFd);
		sock = _sockFd;
	}
	if (!sock) {
		return "";
	}
	return SockUtil::get_local_ip(sock->rawFd());
}

uint16_t Socket::get_local_port() {
	SockFD::Ptr sock;
	{
		lock_guard<spin_mutex> lck(_mtx_sockFd);
		sock = _sockFd;
	}
	if (!sock) {
		return 0;
	}
	return SockUtil::get_local_port(sock->rawFd());

}

string Socket::get_peer_ip() {
	SockFD::Ptr sock;
	{
		lock_guard<spin_mutex> lck(_mtx_sockFd);
		sock = _sockFd;
	}
	if (!sock) {
		return "";
	}
	return SockUtil::get_peer_ip(sock->rawFd());
}

uint16_t Socket::get_peer_port() {
	SockFD::Ptr sock;
	{
		lock_guard<spin_mutex> lck(_mtx_sockFd);
		sock = _sockFd;
	}
	if (!sock) {
		return 0;
	}
	return SockUtil::get_peer_port(sock->rawFd());
}

int Socket::onWrite(const SockFD::Ptr &pSock,bool bMainThread,int flags,bool isUdp) {
	deque<string> sendPktBuf_copy;
	std::shared_ptr<deque<struct sockaddr> > udpSendPeer_copy;
	{
		lock_guard<recursive_mutex> lck(_mtx_sendBuf);
		auto sz = _sendPktBuf.size();
		if (!sz) {
			if (bMainThread) {
				stopWriteEvent(pSock);
				onFlushed(pSock);
			}
			return 0;
		}
		sendPktBuf_copy.swap(_sendPktBuf);
		if(isUdp){
			udpSendPeer_copy.reset(new deque<struct sockaddr>());
			udpSendPeer_copy->swap(_udpSendPeer);
		}
	}
	int byteSent = 0;
	while (sendPktBuf_copy.size()) {
		auto &buf = sendPktBuf_copy.front();
		struct sockaddr *peer;
		if(isUdp){
			peer = &udpSendPeer_copy->front();
		}
		ssize_t n ;
		do {
			if(isUdp){
				n = ::sendto(pSock->rawFd(), buf.data(), buf.size(), flags, peer, sizeof(struct sockaddr));
			}else{
				n = ::send(pSock->rawFd(), buf.data(), buf.size(), flags);
			}
		} while (-1 == n && EINTR == errno);

		if (n > 0) {
			_flushTicker.resetTime();
		}

		byteSent += (n > 0 ? n : 0);

		if (n >= (int) buf.size()) {
			//全部发送成功
			sendPktBuf_copy.pop_front();
			if(isUdp){
				udpSendPeer_copy->pop_front();
			}
			continue;
		}
		if (n <= 0) {
			//一个都没发送成功
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				//InfoL << "send wait...";
				if(!bMainThread){
					startWriteEvent(pSock);
				}
				break;
			}
			//发生异常
			//ErrorL << "send err:" << strerror(err);
			onError(pSock);
			return -1;
		}
		//部分发送成功
		//InfoL << "send some success...";
		if (!bMainThread) {
			startWriteEvent(pSock);
		}
		buf.erase(0, n);
		break;
	} ;

	if (sendPktBuf_copy.empty()) {
		return byteSent + onWrite(pSock,bMainThread,flags,isUdp);
	}
	lock_guard<recursive_mutex> lck(_mtx_sendBuf);
	_sendPktBuf.insert(_sendPktBuf.begin(), sendPktBuf_copy.begin(),sendPktBuf_copy.end());
	if(isUdp){
		_udpSendPeer.insert(_udpSendPeer.begin(), udpSendPeer_copy->begin(),udpSendPeer_copy->end());
	}
	return byteSent;
}


void Socket::startWriteEvent(const SockFD::Ptr &pSock) {
	EventPoller::Instance().modifyEvent(pSock->rawFd(), Event_Read | Event_Error | Event_Write);
}

void Socket::stopWriteEvent(const SockFD::Ptr &pSock) {
	EventPoller::Instance().modifyEvent(pSock->rawFd(), Event_Read | Event_Error);
}
bool Socket::sendTimeout(bool isUdp) {
	if (_flushTicker.elapsedTime() > 5 * 1000) {
		emitErr(SockException(Err_other, "Socket send timeout"));
		_sendPktBuf.clear();
		if(isUdp){
			_udpSendPeer.clear();
		}
		return true;
	}
	return false;
}
void Socket::enableRecv(bool enabled) {
	if(_enableRecv.load() == enabled){
		return;
	}
	_enableRecv = enabled;
	if(!enabled){
		//关闭套接字接收事件
		return;
	}
	//打开套接字接收事件
	weak_ptr<Socket> weakSelf = this->shared_from_this();
	ASYNC_TRACE([weakSelf]() {
		auto strongSelf=weakSelf.lock();
		if (!strongSelf) {
			return;
		}
		SockFD::Ptr sock;
		{
			lock_guard<spin_mutex> lck(strongSelf->_mtx_sockFd);
			sock = strongSelf->_sockFd;
		}
		if(sock){
			//触发读事件，边沿触发才有意义
			strongSelf->onRead(sock,false);
		}
	});

}

}  // namespace Network
}  // namespace ZL



