/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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
#include "Util/onceToken.h"
#include "Util/uv_errno.h"
#include "Util/TimeTicker.h"
#include "Util/ResourcePool.h"
#include "Poller/Timer.h"
#include "Poller/EventPoller.h"
#include "Network/sockutil.h"
#include "Buffer.h"

using namespace std;

namespace toolkit {

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
    
//错误类型枚举
typedef enum {
	Err_success = 0, //成功
	Err_eof, //eof
	Err_timeout, //超时
	Err_refused,//连接别拒绝
	Err_dns,//dns解析失败
	Err_shutdown,//主动关闭
	Err_other = 0xFF,//其他错误
} ErrCode;

//错误信息类
class SockException: public std::exception {
public:
	SockException(ErrCode errCode = Err_success,
                   const string &errMsg = "",
                   int customCode = 0) {
        _errMsg = errMsg;
        _errCode = errCode;
        _customCode = customCode;

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


class SockNum{
public:
    typedef enum {
        Sock_TCP = 0,
        Sock_UDP = 1
    } SockType;

    typedef std::shared_ptr<SockNum> Ptr;
    SockNum(int fd,SockType type){
        _fd = fd;
        _type = type;
    }
    ~SockNum(){
#if defined (OS_IPHONE)
        unsetSocketOfIOS(_fd);
#endif //OS_IPHONE
        ::shutdown(_fd, SHUT_RDWR);
        close(_fd);
    }

    int rawFd() const{
        return _fd;
    }

    SockType type(){
        return _type;
    }

    void setConnected(){
#if defined (OS_IPHONE)
        setSocketOfIOS(_fd);
#endif //OS_IPHONE
    }
private:
    SockType _type;
    int _fd;
#if defined (OS_IPHONE)
    void *readStream=NULL;
    void *writeStream=NULL;
    bool setSocketOfIOS(int socket);
    void unsetSocketOfIOS(int socket);
#endif //OS_IPHONE
};

//socket 文件描述符的包装
//在析构时自动溢出监听并close套接字
//防止描述符溢出
class SockFD : public noncopyable
{
public:
	typedef std::shared_ptr<SockFD> Ptr;
	/**
	 * 创建一个fd对象
	 * @param num 文件描述符，int数字
	 * @param poller 事件监听器
	 */
	SockFD(int num,SockNum::SockType type,const EventPoller::Ptr &poller){
        _num = std::make_shared<SockNum>(num,type);
        _poller = poller;
	}

	/**
	 * 复制一个fd对象
	 * @param that 源对象
	 * @param poller 事件监听器
	 */
    SockFD(const SockFD &that,const EventPoller::Ptr &poller){
        _num = that._num;
        _poller = poller;
        if(_poller == that._poller){
            throw invalid_argument("copy a SockFD with same poller!");
        }
    }

	~SockFD(){
	    auto num = _num;
        _poller->delEvent(_num->rawFd(),[num](bool){});
	}
	void setConnected(){
        _num->setConnected();
	}
	int rawFd() const{
		return _num->rawFd();
	}
    SockNum::SockType type(){
        return _num->type();
    }
private:
    SockNum::Ptr _num;
    EventPoller::Ptr _poller;
};


template <class Mtx = recursive_mutex>
class MutexWrapper {
public:
    MutexWrapper(bool enable){
        _enable = enable;
    }
    ~MutexWrapper(){}

    inline void lock(){
        if(_enable){
            _mtx.lock();
        }
    }
    inline void unlock(){
        if(_enable){
            _mtx.unlock();
        }
    }
private:
    bool _enable;
    Mtx _mtx;
};

//异步IO套接字对象，线程安全的
class Socket: public std::enable_shared_from_this<Socket> ,
              public noncopyable{
public:
    typedef std::shared_ptr<Socket> Ptr;
    //接收数据回调
    typedef function<void(const Buffer::Ptr &buf, struct sockaddr *addr , int addr_len)> onReadCB;
    //发生错误回调
    typedef function<void(const SockException &err)> onErrCB;
    //tcp监听接收到连接请求
    typedef function<void(Socket::Ptr &sock)> onAcceptCB;
	//socket缓存发送完毕回调，通过这个回调可以以最大网速的方式发送数据
    //譬如http文件下载服务器，返回false则移除回调监听
    typedef function<bool()> onFlush;
    //在接收到连接请求前，拦截Socket默认生成方式
    typedef function<Ptr(const EventPoller::Ptr &poller)> onBeforeAcceptCB;

    Socket(const EventPoller::Ptr &poller = nullptr,bool enableMutex = true);
	~Socket();

    //创建tcp客户端，url可以是ip或域名
	void connect(const string &url, uint16_t port,const onErrCB &connectCB, float timeoutSec = 5,const char *localIp = "0.0.0.0",uint16_t localPort = 0);
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
    //设置Socket生成拦截器
    void setOnBeforeAccept(const onBeforeAcceptCB &cb);

    ////////////线程安全的数据发送，udp套接字请传入peerAddr，否则置空////////////
    //发送裸指针数据，内部会把数据拷贝至内部缓存列队，如果要避免数据拷贝，可以调用send(const Buffer::Ptr &buf...）接口
    //返回值:-1代表该socket已经不可用；0代表缓存列队已满，并未产生实质操作(在关闭主动丢包时有效)；否则返回数据长度
    int send(const char *buf, int size = 0,struct sockaddr *addr = nullptr, socklen_t addr_len = 0);
	int send(const string &buf,struct sockaddr *addr = nullptr, socklen_t addr_len = 0);
	int send(string &&buf,struct sockaddr *addr = nullptr, socklen_t addr_len = 0);
	int send(const Buffer::Ptr &buf,struct sockaddr *addr = nullptr, socklen_t addr_len = 0);

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

    //设置发送超时主动断开时间;默认10秒
    void setSendTimeOutSecond(uint32_t second);
    //获取一片缓存
    BufferRaw::Ptr obtainBuffer();
    
    //套接字是否忙，如果套接字写缓存已满则返回true
    bool isSocketBusy() const;

    //获取poller对象
    const EventPoller::Ptr &getPoller() const;

    //从另外一个Socket克隆
    //目的是一个socket可以被多个poller对象监听，提高性能
    bool cloneFromListenSocket(const Socket &other);

    //设置UDP发送数据时的对端地址
    bool setSendPeerAddr(const struct sockaddr *peerAddr);
    //设置发送flags
    void setSendFlags(int flags);

    /**
     * 设置接收缓存
     * @param readBuffer 接收缓存
     */
    void setReadBuffer(const BufferRaw::Ptr &readBuffer);

    /**
     * 关闭套接字
     */
    void closeSock();
private:
    SockFD::Ptr setPeerSock(int fd);
	bool attachEvent(const SockFD::Ptr &pSock,bool isUdp = false);
    int onAccept(const SockFD::Ptr &pSock,int event);
    int onRead(const SockFD::Ptr &pSock,bool isUdp = false);
    void onError(const SockFD::Ptr &pSock);
    void onWriteAble(const SockFD::Ptr &pSock);
    void onConnected(const SockFD::Ptr &pSock, const onErrCB &connectCB);
	void onFlushed(const SockFD::Ptr &pSock);
    void startWriteAbleEvent(const SockFD::Ptr &pSock);
    void stopWriteAbleEvent(const SockFD::Ptr &pSock);
    SockFD::Ptr makeSock(int sock,SockNum::SockType type);
    static SockException getSockErr(const SockFD::Ptr &pSock,bool tryErrno=true);
    bool listen(const SockFD::Ptr &fd);
    bool flushData(const SockFD::Ptr &pSock,bool bPollerThread);
private:
    EventPoller::Ptr _poller;
    std::shared_ptr<Timer> _conTimer;
    SockFD::Ptr _sockFd;
    mutable MutexWrapper<recursive_mutex> _mtx_sockFd;
    /////////////////////
    MutexWrapper<recursive_mutex> _mtx_bufferWaiting;
    List<Buffer::Ptr> _bufferWaiting;
    MutexWrapper<recursive_mutex> _mtx_bufferSending;
    List<BufferList::Ptr> _bufferSending;
    /////////////////////
    MutexWrapper<recursive_mutex> _mtx_event;
    onReadCB _readCB;
    onErrCB _errCB;
    onAcceptCB _acceptCB;
    onFlush _flushCB;
    onBeforeAcceptCB _beforeAcceptCB;
    /////////////////////
    atomic<bool> _enableRecv;
    atomic<bool> _canSendSock;
    //发送超时时间
    uint32_t _sendTimeOutMS = SEND_TIME_OUT_SEC * 1000;
    Ticker _lastFlushTicker;
    int _sock_flags = SOCKET_DEFAULE_FLAGS;
    BufferRaw::Ptr _readBuffer;
    std::shared_ptr<function<void(int)> > _asyncConnectCB;
};

class SocketFlags{
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
    void setPoller(const EventPoller::Ptr &excutor);
    EventPoller::Ptr getPoller();
    //设置socket flags
    SocketHelper &operator << (const SocketFlags &flags);
    //////////////////operator << 系列函数//////////////////
    //发送char *
    SocketHelper &operator << (const char *buf);
    //发送字符串
    SocketHelper &operator << (const string &buf);
    //发送字符串
    SocketHelper &operator << (string &&buf) ;
    //发送Buffer对象
    SocketHelper &operator << (const Buffer::Ptr &buf) ;

    //发送其他类型是数据
    template<typename T>
    SocketHelper &operator << (const T &buf) {
        if(!_sock){
            return *this;
        }
        ostringstream ss;
        ss << buf;
        send(ss.str());
        return *this;
    }

    //////////////////send系列函数//////////////////
    int send(const string &buf);
    int send(string &&buf);
    int send(const char *buf, int size = 0);

    /**
     * 其他send方法、operator << 方法最终都会调用此方法
     * @param buf 数据包
     * @return  -1代表该socket已经不可用；0代表缓存列队已满，并未产生实质操作(在关闭主动丢包时有效)；否则返回数据长度
     */
    virtual int send(const Buffer::Ptr &buf);

    ////////其他方法////////
    //从缓存池中获取一片缓存
    BufferRaw::Ptr obtainBuffer();
    BufferRaw::Ptr obtainBuffer(const void *data,int len);
    //触发onError事件
    virtual void shutdown(const SockException &ex = SockException(Err_shutdown, "self shutdown"));
    /////////获取ip或端口///////////
    const string &get_local_ip();
    uint16_t get_local_port();
    const string &get_peer_ip();
    uint16_t get_peer_port();
    //套接字是否忙，如果套接字写缓存已满则返回true
    bool isSocketBusy() const;

    Task::Ptr async(TaskIn &&task, bool may_sync = true);
    Task::Ptr async_first(TaskIn &&task, bool may_sync = true);
    void sync(TaskIn &&task) ;
    void sync_first(TaskIn &&task);
protected:
    Socket::Ptr _sock;
    EventPoller::Ptr _poller;
private:
    string _local_ip;
    uint16_t _local_port = 0;
    string _peer_ip;
    uint16_t _peer_port = 0;
};


}  // namespace toolkit

#endif /* NETWORK_SOCKET_H */
