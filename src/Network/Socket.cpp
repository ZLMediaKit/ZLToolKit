//
//  Socket.cpp
//  G平台
//
//  Created by boyo on 16/4/13.
//  Copyright © 2016年 boyo. All rights reserved.
//

#include "Socket.hpp"
#include <netdb.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include "Thread/semaphore.hpp"
#include "sockutil.h"
#include "Util/logger.h"
#include "Poller/EventPoller.hpp"
#include "Network/sockutil.h"
using namespace ZL::Thread;
using namespace ZL::Util;
using namespace ZL::Poller;
using namespace ZL::Network;

namespace ZL {
namespace Network {

Socket::Socket() {
	readCB = [](const char *buf,int size,struct sockaddr *) {};
	errCB = [](const SockException &err) {};
	acceptCB = [](Socket_ptr &sock) {};
}
Socket::~Socket() {
	closeSock();
	TraceL << endl;
}

void Socket::setOnRead(onReadCB && cb) {
	if (cb) {
		readCB = cb;
	} else {
		readCB = [](const char *buf,int size,struct sockaddr *) {};
	}
}
void Socket::setOnErr(onErrCB &&cb) {
	if (cb) {
		errCB = cb;
	} else {
		errCB = [](const SockException &err) {};
	}
}
void Socket::setOnAccept(onAcceptCB && cb) {
	if (cb) {
		acceptCB = cb;
	} else {
		acceptCB = [](Socket_ptr &sock) {};
	}
}
void Socket::connect(const string &url, uint16_t port, onErrCB &&connectCB,
		int timeoutSec) {
	closeSock();
	sock = SockUtil::connect(url, port);
	if (sock < 0) {
		connectCB(SockException(Err_other, strerror(errno)));
		return;
	}
	weak_ptr<Socket> weakSelf = this->shared_from_this();
	timedConnector.reset(new Timer(timeoutSec, [weakSelf,connectCB]() {
		auto strongSelf=weakSelf.lock();
		if (!strongSelf) {
			return false;
		}
		SockException err(Err_timeout,strerror(ETIMEDOUT));
		strongSelf->emitErr(err);
		strongSelf->closeSock();
		connectCB(err);
		return false;
	}));
	EventPoller::Instance().addEvent(sock, Event_Write,
			[this,connectCB](int event) {
				onConnected(connectCB);
			});
}

inline void Socket::onConnected(const onErrCB &connectCB) {
	timedConnector.reset();
	auto err = getSockErr(sock);
	if (err.getErrCode() == Err_success) {
		EventPoller::Instance().delEvent(sock);
		attachEvent();
		connectCB(err);
		return;
	}
	emitErr(err);
	closeSock();
	connectCB(err);
}

inline SockException Socket::getSockErr(int fd) {
	int error = -1, len;
	len = sizeof(int);
	getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, (socklen_t *) &len);
	if (error == 0) {
		error = errno;
	}
	switch (error) {
	case 0:
		return SockException(Err_success, strerror(error));
	case ECONNREFUSED:
		return SockException(Err_refused, strerror(error));
	case ETIMEDOUT:
		return SockException(Err_timeout, strerror(error));
	default:
		return SockException(Err_other, strerror(error));
	}
}
void Socket::attachEvent() {
#if defined (__APPLE__)
	setSocketOfIOS(sock);
#endif
	EventPoller::Instance().addEvent(sock,
			Event_Read | Event_Error | Event_Write, [this](int event) {
				if (event & Event_Error) {
					onError();
					return;
				}
				if (event & Event_Read) {
					onRead();
				}
				if (event & Event_Write) {
					onWrite();
				}
			});
}
inline void Socket::onRead() {
	int nread;
	ioctl(sock, FIONREAD, &nread);
	if (nread < 1) {
		emitErr(SockException(Err_eof, "end of file"));
		closeSock();
		return;
	}
	char buf[nread];
	struct sockaddr peerAddr;
	socklen_t len = sizeof(struct sockaddr);
	nread = recvfrom(sock, buf, nread, 0, &peerAddr, &len);
	readCB(buf, nread, &peerAddr);
}
inline void Socket::onError() {
	emitErr(getSockErr(sock));
	closeSock();
}
void Socket::emitErr(const SockException& err) {
	weak_ptr<Socket> weakSelf = this->shared_from_this();
	EventPoller::Instance().sendAsync([weakSelf,err]() {
		auto strongSelf=weakSelf.lock();
		if (!strongSelf) {
			return;
		}
		strongSelf->errCB(err);
	});

}

void Socket::send(const char *buf, int size) {
	string tmp;
	if (size) {
		tmp.assign(buf, size);
	} else {
		tmp.assign(buf);
	}
	send(tmp);
}
void Socket::send(const string &buf) {
	lock_guard<recursive_mutex> lck(mtx_writeBuf);
	if (sock == -1) {
		WarnL << "socket is closed yet";
		emitErr(SockException(Err_other, "socket is closed yet"));
		return;
	}
	if (writeBuf.size() > 32 * 1024) {
		WarnL<< "Socket send buffer overflow,previous data has been discarded.";
		writeBuf.clear();
	}
	writeBuf.append(buf);
#ifndef HAS_EPOLL
	EventPoller::Instance().modifyEvent(sock,
			Event_Read | Event_Error | Event_Write);
#endif //HAS_EPOLL
	onWrite();
}

inline void Socket::onWrite() {
	lock_guard<recursive_mutex> lck(mtx_writeBuf);
	int totalSize = writeBuf.size();
	if (!totalSize) {
#ifndef HAS_EPOLL
		EventPoller::Instance().modifyEvent(sock, Event_Read | Event_Error);
#endif //HAS_EPOLL
		return;
	}
#ifdef __linux__
	ssize_t n = ::send(sock, writeBuf.c_str(), totalSize,
	MSG_NOSIGNAL | MSG_DONTWAIT | MSG_MORE);
#else
	ssize_t n =::send(sock, writeBuf.c_str(), totalSize, MSG_DONTWAIT);
#endif

	if (n >= totalSize) {
		//全部发送成功
		writeBuf.clear();
		return;
	}
	if (n < 0) {
		//一个都没发送成功
		int err = errno;
		if (err == EAGAIN || err == EWOULDBLOCK) {
			//InfoL << "send wait...";
			return;
		}
		//发生异常
		ErrorL << "send err:" << strerror(err);
		onError();
		return;
	}
//部分发送成功
	//InfoL << "send some success...";
	writeBuf.erase(0, n);
}
void Socket::closeSock() {
	if (sock != -1) {
		timedConnector.reset();
		semaphore sem;
		int fd = sock;
		EventPoller::Instance().delEvent(sock, [this,&sem,fd](bool success) {
			if (success) {
#if defined (__APPLE__)
				unsetSocketOfIOS(fd);
#endif
				::close(fd);
			}
			sem.post();
		});
		sem.wait();
		sock = -1;
	}
}

bool Socket::listen(const uint16_t port, const char* localIp, int backLog) {
	closeSock();
	int ret = SockUtil::listen(port, localIp, backLog);
	if (ret == -1) {
		return false;
	}
	if (EventPoller::Instance().addEvent(ret, Event_Read | Event_Error,
			bind(&Socket::onAccept, this, placeholders::_1)) == -1) {
		close(ret);
		WarnL << "开始Poll监听失败";
		return false;
	}
	sock = ret;
	return true;
}
bool Socket::bindUdpSock(const uint16_t port, const char* localIp) {
	closeSock();
	int ret = SockUtil::bindUdpSock(port, localIp);
	if (ret == -1) {
		return false;
	}
	sock = ret;
	attachEvent();
	return true;
}
void Socket::onAccept(int event) {
	if (event & Event_Read) {
		struct sockaddr addr;
		socklen_t len = sizeof addr;
		int peerfd;
#ifdef __linux__
		peerfd = accept4(sock, &addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
#else
		peerfd = accept(sock, &addr, &len);
		if (peerfd > -1) {
			SockUtil::setNoBlocked(peerfd);
		}
#endif //__linux__
		if (peerfd < 0) {
			WarnL << "accept err:" << strerror(errno);
			return;
		}
		Socket_ptr peerSock(new Socket());
		peerSock->setPeerSock(peerfd, &addr);
		acceptCB(peerSock);
	}

	if (event & Event_Error) {
		ErrorL << "tcp服务器监听异常!";
		onError();
	}
}
inline void Socket::setPeerSock(int fd, struct sockaddr *addr) {
	closeSock();
	sock = fd;
	peerAddr = *addr;
	attachEvent();
}

string Socket::get_local_ip() {
	return SockUtil::get_local_ip(sock);
}

uint16_t Socket::get_local_port() {
	return SockUtil::get_local_port(sock);
}

string Socket::get_peer_ip() {
	return SockUtil::get_peer_ip(sock);
}

uint16_t Socket::get_peer_port() {
	return SockUtil::get_peer_port(sock);
}
void Socket::sendTo(const char* buf, int size, struct sockaddr* peerAddr) {
	if (sock == -1) {
		WarnL << "socket is closed yet";
		emitErr(SockException(Err_other, "socket is closed yet"));
		return;
	}
#ifdef __linux__
	::sendto(sock, buf, size, MSG_NOSIGNAL, peerAddr, sizeof(struct sockaddr));
#else
	::sendto(sock, buf, size, 0, peerAddr, sizeof(struct sockaddr));
#endif //__linux__

}

}  // namespace Network
}  // namespace ZL

