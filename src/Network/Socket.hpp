//
//  Socket.hpp
//  xzl
//
//  Created by xzl on 16/4/13.
//

#ifndef Socket_hpp
#define Socket_hpp

#include <stdio.h>
#include <sys/socket.h>
#include <functional>
#include <memory>
#include <string>
#include <deque>
#include <mutex>
#include "Thread/spin_mutex.h"
#include "Poller/Timer.hpp"
#include "Util/TimeTicker.h"

using namespace std;
using namespace ZL::Poller;
using namespace ZL::Util;
using namespace ZL::Thread;

namespace ZL {
namespace Network {

#ifdef MSG_NOSIGNAL
#define FLAG_NOSIGNAL MSG_NOSIGNAL
#else
#define FLAG_NOSIGNAL 0
#endif //MSG_NOSIGNAL

#ifdef MSG_MORE
#define FLAG_MORE MSG_MORE
#else
#define FLAG_MORE 0
#endif //MSG_MORE

#ifdef MSG_DONTWAIT
#define FLAG_DONTWAIT MSG_DONTWAIT
#else
#define FLAG_DONTWAIT 0
#endif //MSG_DONTWAIT

#define TCP_DEFAULE_FLAGS (FLAG_NOSIGNAL | FLAG_DONTWAIT)
#define UDP_DEFAULE_FLAGS (FLAG_NOSIGNAL | FLAG_DONTWAIT)

#define TCP_MAX_SEND_BUF (256*1024)
#define UDP_MAX_SEND_PKT (256)


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
	void reset(ErrCode _errCode, const string &_errMsg) {
		errMsg = _errMsg;
		errCode = _errCode;
	}
	virtual const char* what() const noexcept {
		return errMsg.c_str();
	}

	ErrCode getErrCode() const {
		return errCode;
	}
	operator bool() const{
		return errCode != Err_success;
	}
private:
	string errMsg;
	ErrCode errCode;
};
class SockFD
{
public:
	typedef std::shared_ptr<SockFD> Ptr;
	SockFD(int sock){
		_sock = sock;
	}
	virtual ~SockFD(){
        ::shutdown(_sock, SHUT_RDWR);
#if defined (__APPLE__)
        unsetSocketOfIOS(_sock);
#endif //__APPLE__
        int fd =  _sock;
        EventPoller::Instance().delEvent(fd,[fd](bool){
            ::close(fd);
        });
	}
	void setConnected(){
#if defined (__APPLE__)
		setSocketOfIOS(_sock);
#endif //__APPLE__
	}
	int rawFd() const{
		return _sock;
	}
private:
	int _sock;

#if defined (__APPLE__)
	void *readStream=NULL;
	void *writeStream=NULL;
	bool setSocketOfIOS(int socket);
	void unsetSocketOfIOS(int socket);
#endif //__APPLE__
};

class Socket: public std::enable_shared_from_this<Socket> {
public:
	class Buffer {
	public:
		typedef std::shared_ptr<Buffer> Ptr;
		Buffer(uint32_t size) {
			_size = size;
			_data = new char[size];
		}
		virtual ~Buffer() {
			delete[] _data;
		}
		const char *data() const {
			return _data;
		}
		uint32_t size() const {
			return _size;
		}
	private:
		friend class Socket;
		char *_data;
		uint32_t _size;
	};
	typedef std::shared_ptr<Socket> Ptr;
	typedef function<void(const Buffer::Ptr &buf, struct sockaddr *addr)> onReadCB;
	typedef function<void(const SockException &err)> onErrCB;
	typedef function<void(Socket::Ptr &sock)> onAcceptCB;
	typedef function<bool()> onFlush;

	Socket();
	virtual ~Socket();
	int rawFD() const{
		auto sock = _sockFd;
		if(!sock){
			return -1;
		}
		return sock->rawFd();
	}
	void connect(const string &url, uint16_t port, onErrCB &&connectCB, int timeoutSec = 5);
	bool listen(const uint16_t port, const char *localIp = "0.0.0.0", int backLog = 1024);
	bool bindUdpSock(const uint16_t port, const char *localIp = "0.0.0.0");

	void setOnRead(const onReadCB &cb);
	void setOnErr(const onErrCB &cb);
	void setOnAccept(const onAcceptCB &cb);
	void setOnFlush(const onFlush &cb);

	int send(const char *buf, int size = 0,int flags = TCP_DEFAULE_FLAGS);
	int send(const string &buf,int flags = TCP_DEFAULE_FLAGS);
	int sendTo(const char *buf, int size, struct sockaddr *peerAddr,int flags = UDP_DEFAULE_FLAGS);
	int sendTo(const string &buf, struct sockaddr *peerAddr,int flags = UDP_DEFAULE_FLAGS);
	bool emitErr(const SockException &err);

	string get_local_ip();
	uint16_t get_local_port();
	string get_peer_ip();
	uint16_t get_peer_port();

	void setTcpBufSize(uint32_t iBufSize){
		_iTcpMaxBufSize = iBufSize;
	}
	void setUdpPktSize(uint32_t iPktSize){
		_iUdpMaxPktSize = iPktSize;
	}
private:
	spin_mutex _mtx_sockFd;
	SockFD::Ptr _sockFd;
	//send buffer
	recursive_mutex _mtx_sendBuf;
	string _tcpSendBuf;
	deque<string> _udpSendBuf;
	deque<struct sockaddr> _udpSendPeer;
	/////////////////////
	std::shared_ptr<Timer> _conTimer;
	struct sockaddr _peerAddr;
	recursive_mutex _mtx_read;
    recursive_mutex _mtx_err;
    recursive_mutex _mtx_accept;
    recursive_mutex _mtx_flush;
	onReadCB _readCB;
	onErrCB _errCB;
	onAcceptCB _acceptCB;
	onFlush _flushCB;
	Ticker _flushTicker;
    int _lastTcpFlags = TCP_DEFAULE_FLAGS;
    int _lastUdpFlags = UDP_DEFAULE_FLAGS;
    uint32_t _iTcpMaxBufSize = TCP_MAX_SEND_BUF;
    uint32_t _iUdpMaxPktSize = UDP_MAX_SEND_PKT;

	void closeSock();
	bool setPeerSock(int fd, struct sockaddr *addr);
	bool attachEvent(const SockFD::Ptr &pSock,bool tcp = true);

	int onAccept(const SockFD::Ptr &pSock,int event);
	int onRead(const SockFD::Ptr &pSock,bool mayEof=true);
	void onError(const SockFD::Ptr &pSock);
	int onWriteTCP(const SockFD::Ptr &pSock, bool bMainThread,int flags);
	int onWriteUDP(const SockFD::Ptr &pSock, bool bMainThread,int flags);
	void onConnected(const SockFD::Ptr &pSock, const onErrCB &connectCB);
	void onFlushed(const SockFD::Ptr &pSock);

	void startWriteEvent(const SockFD::Ptr &pSock);
	void stopWriteEvent(const SockFD::Ptr &pSock);
	bool sendTimeout();
	SockFD::Ptr makeSock(int sock){
		return std::make_shared<SockFD>(sock);
	}
	static SockException getSockErr(const SockFD::Ptr &pSock,bool tryErrno=true);

};

}  // namespace Network
}  // namespace ZL

#endif /* Socket_hpp */
