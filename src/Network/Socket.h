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

#ifndef NETWORK_SOCKET_H
#define NETWORK_SOCKET_H

#include <memory>
#include <string>
#include <deque>
#include <mutex>
#include <vector>
#include <atomic>
#include <sstream>
#include <functional>
#include "Util/util.h"
#include "Util/uv_errno.h"
#include "Util/TimeTicker.h"
#include "Util/ResourcePool.h"
#include "Poller/Timer.h"
#include "Network/sockutil.h"
#include "Thread/spin_mutex.h"

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

//默认的socket flags:不触发SIGPIPE,非阻塞发送
#define SOCKET_DEFAULE_FLAGS (FLAG_NOSIGNAL | FLAG_DONTWAIT )
    
//发送超时时间，如果在规定时间内一直没有发送数据成功，那么将触发onErr事件
#define SEND_TIME_OUT_SEC 10
    
//缓存列队中数据最大保存秒数,默认最大保存5秒的数据
#define SEND_BUF_MAX_SEC 5

#if defined(__APPLE__)
  #import "TargetConditionals.h"
  #if TARGET_IPHONE_SIMULATOR
    #define OS_IPHONE
  #elif TARGET_OS_IPHONE
    #define OS_IPHONE
  #endif
#endif //__APPLE__

//错误类型枚举
typedef enum {
	Err_success = 0, //成功
	Err_eof, //eof
	Err_timeout, //超时
	Err_refused,//连接别拒绝
	Err_dns,//dns解析失败
	Err_other,//其他错误
} ErrCode;

//错误信息类
class SockException: public std::exception {
public:
	SockException(ErrCode errCode = Err_success,
                  const string &errMsg = "") {
		_errMsg = errMsg;
		_errCode = errCode;
	}
    //重置错误
	void reset(ErrCode errCode, const string &errMsg) {
		_errMsg = errMsg;
        _errCode = errCode;
	}
    //错误提示
	virtual const char* what() const noexcept {
		return _errMsg.c_str();
	}
    //错误代码
	ErrCode getErrCode() const {
		return _errCode;
	}
    //判断是否真的有错
	operator bool() const{
		return _errCode != Err_success;
	}
    //用户自定义错误代码
    int getCustomCode () const{
        return _customCode;
    }
    //获取用户自定义错误代码
    void setCustomCode(int code) {
        _customCode = code;
    };
private:
	string _errMsg;
	ErrCode _errCode;
    int _customCode = 0;
};

//禁止拷贝基类
class noncopyable
{
protected:
  noncopyable() {}
  ~noncopyable() {}
private:
    //禁止拷贝
    noncopyable(const noncopyable &that) = delete;
    noncopyable(noncopyable &&that) = delete;
    noncopyable &operator=(const noncopyable &that) = delete;
    noncopyable &operator=(noncopyable &&that) = delete;
};
//socket 文件描述符的包装
//在析构时自动溢出监听并close套接字
//防止描述符溢出
class SockFD : public noncopyable
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

//缓存基类
class Buffer : public noncopyable {
public:
    typedef std::shared_ptr<Buffer> Ptr;
    Buffer(){};
    virtual ~Buffer(){};
    //返回数据长度
    virtual char *data() = 0 ;
    virtual uint32_t size() const = 0;
};

//指针式缓存对象，
class BufferRaw : public Buffer{
public:
    typedef std::shared_ptr<BufferRaw> Ptr;
    BufferRaw(uint32_t capacity = 0) {
        if(capacity){
            setCapacity(capacity);
        }
    }
    virtual ~BufferRaw() {
        if(_data){
            delete [] _data;
        }
    }
    //在写入数据时请确保内存是否越界
    char *data() override {
        return _data;
    }
    //有效数据大小
    uint32_t size() const override{
        return _size;
    }
    //分配内存大小
    void setCapacity(uint32_t capacity){
        if(_data){
            delete [] _data;
        }
        _data = new char[capacity];
        _capacity = capacity;
    }
    //设置有效数据大小
    void setSize(uint32_t size){
        if(size > _capacity){
            throw std::invalid_argument("Buffer::setSize out of range");
        }
        _size = size;
    }
    //赋值数据
    void assign(const char *data,int size = 0){
        if(size <=0 ){
            size = strlen(data);
        }
        setCapacity(size);
        memcpy(_data,data,size);
        setSize(size);
    }
private:
    char *_data = nullptr;
    int _capacity = 0;
    int _size = 0;
};

class Packet : public noncopyable
{
public:
    typedef std::shared_ptr<Packet> Ptr;
    Packet(){}
    ~Packet(){  if(_addr) delete _addr;  }

    void updateStamp();
    uint32_t getStamp() const;
    void setAddr(const struct sockaddr *addr);
    void setFlag(int flag){ _flag = flag; }
    void setData(const Buffer::Ptr &data){
        _data = data;
        _offset = 0;
    }
    int send(int fd);

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
    uint32_t _stamp = 0;
};

//字符串缓存
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

//异步IO套接字对象，线程安全的
class Socket: public std::enable_shared_from_this<Socket> , public noncopyable {
public:
    typedef std::shared_ptr<Socket> Ptr;
    //接收数据回调
    typedef function<void(const Buffer::Ptr &buf, struct sockaddr *addr)> onReadCB;
    //发生错误回调
    typedef function<void(const SockException &err)> onErrCB;
    //tcp监听接收到连接请求
    typedef function<void(Socket::Ptr &sock)> onAcceptCB;
	//socket缓存发送完毕回调，通过这个回调可以以最大网速的方式发送数据
    //譬如http文件下载服务器，返回false则移除回调监听
    typedef function<bool()> onFlush;

	Socket();
	virtual ~Socket();

    //创建tcp客户端，url可以是ip或域名
	void connect(const string &url, uint16_t port,const onErrCB &connectCB, float timeoutSec = 5);
    //创建tcp监听
    bool listen(const uint16_t port, const char *localIp = "0.0.0.0", int backLog = 1024);
    //创建udp套接字,udp是无连接的，所以可以作为服务器和客户端；port为0则随机分配端口
    bool bindUdpSock(const uint16_t port, const char *localIp = "0.0.0.0");

    ////////////设置事件回调////////////
    //收到数据后回调,tcp或udp客户端有效
	void setOnRead(const onReadCB &cb);
    //收到err事件回调，包括eof等
	void setOnErr(const onErrCB &cb);
    //tcp监听接收到连接请求回调
	void setOnAccept(const onAcceptCB &cb);
    //socket缓存发送完毕回调，通过这个回调可以以最大网速的方式发送数据
    //譬如http文件下载服务器，返回false则移除回调监听
	void setOnFlush(const onFlush &cb);

    ////////////线程安全的数据发送，udp套接字请传入peerAddr，否则置空////////////
    //发送裸指针数据，内部会把数据拷贝至内部缓存列队，如果要避免数据拷贝，可以调用send(const Buffer::Ptr &buf...）接口
    //返回值:-1代表该socket已经不可用；0代表缓存列队已满，并未产生实质操作(在关闭主动丢包时有效)；否则返回数据长度
    int send(const char *buf, int size = 0,int flags = SOCKET_DEFAULE_FLAGS,struct sockaddr *peerAddr = nullptr);
	int send(const string &buf,int flags = SOCKET_DEFAULE_FLAGS,struct sockaddr *peerAddr = nullptr);
	int send(string &&buf,int flags = SOCKET_DEFAULE_FLAGS,struct sockaddr *peerAddr = nullptr);
	int send(const Buffer::Ptr &buf,int flags = SOCKET_DEFAULE_FLAGS , struct sockaddr *peerAddr = nullptr);

    //关闭socket且触发onErr回调，onErr回调将在主线程中进行
	bool emitErr(const SockException &err);
    //关闭或开启数据接收
	void enableRecv(bool enabled);
    //获取裸文件描述符，请勿进行close操作(因为Socket对象会管理其生命周期)
    int rawFD() const;
    //获取本机ip，多网卡时比较有用
	string get_local_ip();
    //获取本机端口号
	uint16_t get_local_port();
    //获取对方ip
	string get_peer_ip();
    //获取对方端口号
	uint16_t get_peer_port();

    //设置缓存列队长度
    //譬如调用Socket::send若干次，那么这些数据可能并不会真正发送到网络
    //而是会缓存在Socket对象的缓存列队中
    //如果缓存列队中最老的数据包寿命超过最大限制，那么调用Socket::send将返回0
	void setSendBufSecond(uint32_t second);
    //设置发送超时主动断开时间;默认10秒
    void setSendTimeOutSecond(uint32_t second);
    //获取一片缓存
    BufferRaw::Ptr obtainBuffer();
    
    //套接字是否忙，如果套接字写缓存已满则返回true
    bool isSocketBusy() const;
private:
    void closeSock();
    bool setPeerSock(int fd);
	bool attachEvent(const SockFD::Ptr &pSock,bool isUdp = false);
    int onAccept(const SockFD::Ptr &pSock,int event);
    int onRead(const SockFD::Ptr &pSock,bool mayEof=true);
    void onError(const SockFD::Ptr &pSock);
    void onWriteAble(const SockFD::Ptr &pSock);
    bool sendData(const SockFD::Ptr &pSock, bool bMainThread);
    void onConnected(const SockFD::Ptr &pSock, const onErrCB &connectCB);
	void onFlushed(const SockFD::Ptr &pSock);
    void startWriteAbleEvent(const SockFD::Ptr &pSock);
    void stopWriteAbleEvent(const SockFD::Ptr &pSock);
    bool sendTimeout();
    SockFD::Ptr makeSock(int sock);
    uint32_t getBufSecondLength();
    static SockException getSockErr(const SockFD::Ptr &pSock,bool tryErrno=true);
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
    atomic<bool> _enableRecv;
    atomic<bool> _canSendSock;
    //发送超时时间
    uint32_t _sendTimeOutSec = SEND_TIME_OUT_SEC;
    uint32_t _sendBufSec = SEND_BUF_MAX_SEC;
};

class  SocketFlags{
public:
    SocketFlags(int flags):_flags(flags){};
    ~SocketFlags(){}
    int _flags;
};

//套接字以cout的方式写数据等工具
class SocketHelper {
public:
    SocketHelper(const Socket::Ptr &sock);
    virtual ~SocketHelper();
    //重新设置socket
    void setSock(const Socket::Ptr &sock);
    //设置socket flags
    virtual SocketHelper &operator << (const SocketFlags &flags);
    //////////////////operator << 系列函数//////////////////
    //发送char *
    virtual SocketHelper &operator << (const char *buf);
    //发送字符串
    virtual SocketHelper &operator << (const string &buf);
    //发送字符串
    virtual SocketHelper &operator << (string &&buf) ;
    //发送Buffer对象
    virtual SocketHelper &operator << (const Buffer::Ptr &buf) ;

    //发送其他类型是数据
    template<typename T>
    SocketHelper &operator << (const T &buf) {
        if(!_sock){
            return *this;
        }
        ostringstream ss;
        ss << buf;
        _sock->send(ss.str(),_flags);
        return *this;
    }

    //////////////////send系列函数//////////////////
    virtual int send(const string &buf);
    virtual int send(string &&buf);
    virtual int send(const char *buf, int size);
    virtual int send(const Buffer::Ptr &buf);

    ////////其他方法////////
    //从缓存池中获取一片缓存
    virtual BufferRaw::Ptr obtainBuffer();
    //触发onError事件
    virtual void shutdown();
    /////////获取ip或端口///////////
    const string &get_local_ip() const;
    uint16_t get_local_port();
    const string &get_peer_ip() const;
    uint16_t get_peer_port();
    //套接字是否忙，如果套接字写缓存已满则返回true
    bool isSocketBusy() const;
protected:
    int _flags = SOCKET_DEFAULE_FLAGS;
    Socket::Ptr _sock;
private:
    string _local_ip;
    uint16_t _local_port = 0;
    string _peer_ip;
    uint16_t _peer_port = 0;
};


}  // namespace Network
}  // namespace ZL

#endif /* NETWORK_SOCKET_H */
