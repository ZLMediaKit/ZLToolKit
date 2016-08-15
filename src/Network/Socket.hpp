//
//  Socket.hpp
//  G平台
//
//  Created by boyo on 16/4/13.
//  Copyright © 2016年 boyo. All rights reserved.
//

#ifndef Socket_hpp
#define Socket_hpp

#include <stdio.h>
#include <sys/socket.h>
#include <functional>
#include <memory>
#include <string>
#include "Poller/Timer.hpp"
using namespace std;
using namespace ZL::Poller;

namespace ZL {
namespace Network {
class Socket;
typedef shared_ptr<Socket> Socket_ptr;

typedef enum {
	Err_success = 0, //成功
	Err_eof, //eof
	Err_timeout, //超时
	Err_refused,
	Err_dns,
	Err_other,
} ErrCode;

class SockException: public std::exception {
public:
	SockException(ErrCode _errCode, const string &_errMsg) {
		errMsg = _errMsg;
		errCode = _errCode;
	}
	virtual const char* what() const noexcept {
		return errMsg.c_str();
	}

	ErrCode getErrCode() const {
		return errCode;
	}
private:
	string errMsg;
	ErrCode errCode;
};

typedef function<void(const char *buf, int size,struct sockaddr *addr)> onReadCB;
typedef function<void(const SockException &err)> onErrCB;
typedef function<void(Socket_ptr &sock)> onAcceptCB;

class Socket: public std::enable_shared_from_this<Socket> {
public:
	Socket();
	virtual ~Socket();
	void connect(const string &url, uint16_t port, onErrCB &&connectCB,
			int timeoutSec = 5);
	bool listen(const uint16_t port, const char *localIp = "0.0.0.0",
			int backLog = 1024);
	bool bindUdpSock(const uint16_t port, const char *localIp = "0.0.0.0");
	void setOnRead(onReadCB && cb);
	void setOnErr(onErrCB &&cb);
	void setOnAccept(onAcceptCB && cb);

	void send(const char *buf, int size = 0);
	void send(const string &buf);
	void sendTo(const char *buf, int size,struct sockaddr *peerAddr);
	void emitErr(const SockException &err);

	string get_local_ip();
	uint16_t get_local_port();
	string get_peer_ip();
	uint16_t get_peer_port();
private:
	int sock = -1;
	string writeBuf;
	recursive_mutex mtx_writeBuf;
	shared_ptr<Timer> timedConnector;
	struct sockaddr peerAddr;
#if defined (__APPLE__)
	void *readStream=NULL;
	void *writeStream=NULL;
	bool setSocketOfIOS(int m_socket);
	void unsetSocketOfIOS(int m_socket);
#endif

	onReadCB readCB;
	onErrCB errCB;
	onAcceptCB acceptCB;
	void closeSock();
	void onAccept(int event);
	inline void setPeerSock(int fd, struct sockaddr *addr);
	void attachEvent();
	inline void onRead();
	inline void onError();
	inline void onWrite();
	inline void onConnected(const onErrCB &connectCB);
	static inline SockException getSockErr(int fd);

};

}  // namespace Network
}  // namespace ZL

#endif /* Socket_hpp */
