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

#ifndef Socket_h
#define Socket_h

#include <memory>
#include <string>
#include <deque>
#include <mutex>
#include <vector>
#include <atomic>
#include <functional>
#include "Util/util.h"
#include "Util/TimeTicker.h"
#include "Poller/Timer.h"
#include "Network/sockutil.h"
#include "Thread/spin_mutex.h"
#include "Util/uv_errno.h"
#include <Util/ResourcePool.h>

using namespace std;
using namespace ZL::Util;
using namespace ZL::Poller;
using namespace ZL::Thread;

namespace ZL {
namespace Network {

#if defined(MSG_NOSIGNAL)
#define FLAG_NOSIGNAL MSG_NOSIGNAL
#else
#define FLAG_NOSIGNAL 0
#endif //MSG_NOSIGNAL

#if defined(MSG_MORE)
#define FLAG_MORE MSG_MORE
#else
#define FLAG_MORE 0
#endif //MSG_MORE

#if defined(MSG_DONTWAIT)
#define FLAG_DONTWAIT MSG_DONTWAIT
#else
#define FLAG_DONTWAIT 0
#endif //MSG_DONTWAIT

#define SOCKET_DEFAULE_FLAGS (FLAG_NOSIGNAL | FLAG_DONTWAIT )

#define MAX_SEND_PKT (256)

#if defined(__APPLE__)
  #import "TargetConditionals.h"
  #if TARGET_IPHONE_SIMULATOR
    #define OS_IPHONE
  #elif TARGET_OS_IPHONE
    #define OS_IPHONE
  #endif
#endif //__APPLE__

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
	SockException(ErrCode _errCode = Err_success, const string &_errMsg = "") {
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
#if defined (OS_IPHONE)
        unsetSocketOfIOS(_sock);
#endif //OS_IPHONE
        int fd =  _sock;
        EventPoller::Instance().delEvent(fd,[fd](bool){
            close(fd);
        });
	}
	void setConnected(){
#if defined (OS_IPHONE)
		setSocketOfIOS(_sock);
#endif //OS_IPHONE
	}
	int rawFd() const{
		return _sock;
	}
private:
	int _sock;

#if defined (OS_IPHONE)
	void *readStream=NULL;
	void *writeStream=NULL;
	bool setSocketOfIOS(int socket);
	void unsetSocketOfIOS(int socket);
#endif //OS_IPHONE
};

class Socket: public std::enable_shared_from_this<Socket> {
public:
	class Buffer
	{
    private:
        Buffer(const Buffer &that) = delete;
        Buffer(Buffer &&that) = delete;
        Buffer &operator=(const Buffer &that) = delete;
        Buffer &operator=(Buffer &&that) = delete;
	public:
		typedef std::shared_ptr<Buffer> Ptr;
		Buffer(){};
		virtual ~Buffer(){};
		virtual char *data() = 0 ;
		virtual uint32_t size() const = 0;
	};

	class BufferRaw : public  Buffer{
	public:
		typedef std::shared_ptr<BufferRaw> Ptr;
		BufferRaw(uint32_t capacity = 0) {
			setCapacity(capacity);
		}
		virtual ~BufferRaw() {
			if(_data){
				delete[] _data;
			}
		}
		char *data() override {
			return _data;
		}
		uint32_t size() const override{
			return _size;
		}
		void setCapacity(uint32_t capacity){
			if(_capacity >= capacity){
				return;
			}
			if(_data){
				delete[] _data;
			}
			_data = new char[capacity];;
			_capacity = capacity;
		}
		void setSize(uint32_t size){
			if(size > _capacity){
				throw std::invalid_argument("Buffer::setSize out of range");
			}
			_size = size;
		}
		void assign(const char *data,int size){
			setCapacity(size);
			memcpy(_data,data,size);
			setSize(size);
		}
	private:
		char *_data = nullptr;
		int _size = 0;
		int _capacity = 0;
	};

    class BufferString : public  Buffer {
    public:
        typedef std::shared_ptr<BufferString> Ptr;
        BufferString(const string &data):_data(data) {}
        BufferString(string &&data):_data(std::move(data)){}
        virtual ~BufferString() {}
        char *data() override {
            return const_cast<char *>(_data.data());
        }
        uint32_t size() const override{
            return _data.size();
        }
    private:
        string _data;
    };

	typedef std::shared_ptr<Socket> Ptr;
	typedef function<void(const Buffer::Ptr &buf, struct sockaddr *addr)> onReadCB;
	typedef function<void(const SockException &err)> onErrCB;
	typedef function<void(Socket::Ptr &sock)> onAcceptCB;
	typedef function<bool()> onFlush;

	Socket();
	virtual ~Socket();
	int rawFD() const{
		SockFD::Ptr sock;
		{
			lock_guard<spin_mutex> lck(_mtx_sockFd);
			sock = _sockFd;
		}
		if(!sock){
			return -1;
		}
		return sock->rawFd();
	}
	void connect(const string &url, uint16_t port,const onErrCB &connectCB, int timeoutSec = 5);
	bool listen(const uint16_t port, const char *localIp = "0.0.0.0", int backLog = 1024);
	bool bindUdpSock(const uint16_t port, const char *localIp = "0.0.0.0");

	void setOnRead(const onReadCB &cb);
	void setOnErr(const onErrCB &cb);
	void setOnAccept(const onAcceptCB &cb);
	void setOnFlush(const onFlush &cb);

	int send(const char *buf, int size = 0,int flags = SOCKET_DEFAULE_FLAGS,struct sockaddr *peerAddr = nullptr);
	int send(const string &buf,int flags = SOCKET_DEFAULE_FLAGS,struct sockaddr *peerAddr = nullptr);
	int send(string &&buf,int flags = SOCKET_DEFAULE_FLAGS,struct sockaddr *peerAddr = nullptr);
	int send(const Buffer::Ptr &buf,int flags = SOCKET_DEFAULE_FLAGS , struct sockaddr *peerAddr = nullptr);

	bool emitErr(const SockException &err);
	void enableRecv(bool enabled);

	string get_local_ip();
	uint16_t get_local_port();
	string get_peer_ip();
	uint16_t get_peer_port();

	void setSendPktSize(uint32_t iPktSize){
		_iMaxSendPktSize = iPktSize;
        _bufferPool.reSize(iPktSize);
        _packetPool.reSize(iPktSize);
	}
    void setShouldDropPacket(bool dropPacket){
        _shouldDropPacket = dropPacket;
    }

private:
    class Packet
    {
    private:
        Packet(const Packet &that) = delete;
        Packet(Packet &&that) = delete;
        Packet &operator=(const Packet &that) = delete;
        Packet &operator=(Packet &&that) = delete;
    public:
        typedef std::shared_ptr<Packet> Ptr;
		Packet(){}
        ~Packet(){  if(_addr) delete _addr;  }

        void setAddr(const struct sockaddr *addr){
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
        void setFlag(int flag){
            _flag = flag;
        }
        void setData(const Buffer::Ptr &data){
            _data = data;
            _offset = 0;
        }
        int send(int fd){
            int n;
            do {
                if(_addr){
                    n = ::sendto(fd, _data->data() + _offset, _data->size() - _offset, _flag, _addr, sizeof(struct sockaddr));
                }else{
                    n = ::send(fd, _data->data() + _offset, _data->size() - _offset, _flag);
                }
            } while (-1 == n && UV_EINTR == get_uv_error(true));

            if(n >= _data->size() - _offset){
                //全部发送成功
                _offset = _data->size();
                _data.reset();
            }else if(n > 0) {
                //部分发送成功
                _offset += n;
            }
            return n;
        }
        bool empty() const{
            if(!_data){
                return true;
            }
            return _offset >= _data->size();
        }
    private:
        struct sockaddr *_addr = nullptr;
        Buffer::Ptr _data;
        int _flag = 0;
        int _offset = 0;
    };

private:
 	mutable spin_mutex _mtx_sockFd;
	SockFD::Ptr _sockFd;
	recursive_mutex _mtx_sendBuf;
	deque<Packet::Ptr> _sendPktBuf;
	/////////////////////
	std::shared_ptr<Timer> _conTimer;
	spin_mutex _mtx_read;
	spin_mutex _mtx_err;
	spin_mutex _mtx_accept;
	spin_mutex _mtx_flush;
	onReadCB _readCB;
	onErrCB _errCB;
	onAcceptCB _acceptCB;
	onFlush _flushCB;
	Ticker _flushTicker;
    uint32_t _iMaxSendPktSize = MAX_SEND_PKT;
    atomic<bool> _enableRecv;
    //默认网络底层可以主动丢包
    bool _shouldDropPacket = true;

	void closeSock();
	bool setPeerSock(int fd);
	bool attachEvent(const SockFD::Ptr &pSock,bool isUdp = false);

	int onAccept(const SockFD::Ptr &pSock,int event);
	int onRead(const SockFD::Ptr &pSock,bool mayEof=true);
	void onError(const SockFD::Ptr &pSock);
	bool onWrite(const SockFD::Ptr &pSock, bool bMainThread);
	void onConnected(const SockFD::Ptr &pSock, const onErrCB &connectCB);
	void onFlushed(const SockFD::Ptr &pSock);

	void startWriteEvent(const SockFD::Ptr &pSock);
	void stopWriteEvent(const SockFD::Ptr &pSock);
	bool sendTimeout();
	SockFD::Ptr makeSock(int sock){
		return std::make_shared<SockFD>(sock);
	}
	static SockException getSockErr(const SockFD::Ptr &pSock,bool tryErrno=true);
    ResourcePool<BufferRaw,MAX_SEND_PKT> _bufferPool;
    ResourcePool<Packet,MAX_SEND_PKT> _packetPool;
};

}  // namespace Network
}  // namespace ZL

#endif /* Socket_h */
