/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef NETWORK_SOCKET_H
#define NETWORK_SOCKET_H

#include <memory>
#include <string>
#include <mutex>
#include <atomic>
#include <sstream>
#include <functional>
#include "Util/SpeedStatistic.h"
#include "sockutil.h"
#include "Poller/Timer.h"
#include "Poller/EventPoller.h"
#include "BufferSock.h"

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

//默认的socket flags:不触发SIGPIPE,非阻塞发送  [AUTO-TRANSLATED:fefc4946]
//Default socket flags: do not trigger SIGPIPE, non-blocking send
#define SOCKET_DEFAULE_FLAGS (FLAG_NOSIGNAL | FLAG_DONTWAIT )
    
//发送超时时间，如果在规定时间内一直没有发送数据成功，那么将触发onErr事件  [AUTO-TRANSLATED:9c5d8d87]
//Send timeout time, if no data is sent successfully within the specified time, the onErr event will be triggered
#define SEND_TIME_OUT_SEC 10
    
//错误类型枚举  [AUTO-TRANSLATED:c85ff6f6]
//Error type enumeration
typedef enum {
    Err_success = 0, //成功 success
    Err_eof, //eof
    Err_timeout, //超时 socket timeout
    Err_refused,//连接被拒绝 socket refused
    Err_reset,//连接被重置  socket reset
    Err_dns,//dns解析失败 dns resolve failed
    Err_shutdown,//主动关闭 socket shutdown
    Err_other = 0xFF,//其他错误 other error
} ErrCode;

//错误信息类  [AUTO-TRANSLATED:5d337296]
//Error message class
class SockException : public std::exception {
public:
    SockException(ErrCode code = Err_success, const std::string &msg = "", int custom_code = 0) {
        _msg = msg;
        _code = code;
        _custom_code = custom_code;
    }

    //重置错误  [AUTO-TRANSLATED:d421942a]
    //Reset error
    void reset(ErrCode code, const std::string &msg, int custom_code = 0) {
        _msg = msg;
        _code = code;
        _custom_code = custom_code;
    }

    //错误提示  [AUTO-TRANSLATED:989d5b29]
    //Error prompt
    const char *what() const noexcept override {
        return _msg.c_str();
    }

    //错误代码  [AUTO-TRANSLATED:06930b2e]
    //Error code
    ErrCode getErrCode() const {
        return _code;
    }

    //用户自定义错误代码  [AUTO-TRANSLATED:aef77c4e]
    //User-defined error code
    int getCustomCode() const {
        return _custom_code;
    }

    //判断是否真的有错  [AUTO-TRANSLATED:b12fad69]
    //Determine if there is really an error
    operator bool() const {
        return _code != Err_success;
    }

private:
    ErrCode _code;
    int _custom_code = 0;
    std::string _msg;
};

//std::cout等输出流可以直接输出SockException对象  [AUTO-TRANSLATED:9b0a61e5]
//std::cout and other output streams can directly output SockException objects
std::ostream &operator<<(std::ostream &ost, const SockException &err);

class SockNum {
public:
    using Ptr = std::shared_ptr<SockNum>;

    typedef enum {
        Sock_Invalid = -1,
        Sock_TCP = 0,
        Sock_UDP = 1,
        Sock_TCP_Server = 2
    } SockType;

    SockNum(int fd, SockType type) {
        _fd = fd;
        _type = type;
    }

    ~SockNum() {
#if defined (OS_IPHONE)
        unsetSocketOfIOS(_fd);
#endif //OS_IPHONE
        // 停止socket收发能力  [AUTO-TRANSLATED:73526f07]
        //Stop socket send and receive capability
        #if defined(_WIN32)
        ::shutdown(_fd, SD_BOTH);
        #else
        ::shutdown(_fd, SHUT_RDWR);
        #endif
        close(_fd);
    }

    int rawFd() const {
        return _fd;
    }

    SockType type() {
        return _type;
    }

    void setConnected() {
#if defined (OS_IPHONE)
        setSocketOfIOS(_fd);
#endif //OS_IPHONE
    }

#if defined (OS_IPHONE)
private:
    void *readStream=nullptr;
    void *writeStream=nullptr;
    bool setSocketOfIOS(int socket);
    void unsetSocketOfIOS(int socket);
#endif //OS_IPHONE

private:
    int _fd;
    SockType _type;
};

//socket 文件描述符的包装  [AUTO-TRANSLATED:d6705c7a]
//Socket file descriptor wrapper
//在析构时自动溢出监听并close套接字  [AUTO-TRANSLATED:3d9c96d9]
//Automatically overflow listening and close socket when destructing
//防止描述符溢出  [AUTO-TRANSLATED:17c2f2f0]
//Prevent descriptor overflow
class SockFD : public noncopyable {
public:
    using Ptr = std::shared_ptr<SockFD>;

    /**
     * 创建一个fd对象
     * @param num 文件描述符，int数字
     * @param poller 事件监听器
     * Create an fd object
     * @param num File descriptor, int number
     * @param poller Event listener
     
     * [AUTO-TRANSLATED:2eb468c4]
     */
    SockFD(SockNum::Ptr num, const EventPoller::Ptr &poller) {
        _num = std::move(num);
        _poller = poller;
    }

    /**
     * 复制一个fd对象
     * @param that 源对象
     * @param poller 事件监听器
     * Copy an fd object
     * @param that Source object
     * @param poller Event listener
     
     * [AUTO-TRANSLATED:51fca132]
     */
    SockFD(const SockFD &that, const EventPoller::Ptr &poller) {
        _num = that._num;
        _poller = poller;
        if (_poller == that._poller) {
            throw std::invalid_argument("Copy a SockFD with same poller");
        }
    }

     ~SockFD() { delEvent(); }

    void delEvent() {
        if (_poller) {
            auto num = _num;
            // 移除io事件成功后再close fd  [AUTO-TRANSLATED:4b5e429f]
            //Remove IO event successfully before closing fd
            _poller->delEvent(num->rawFd(), [num](bool) {});
            _poller = nullptr;
        }
    }

    void setConnected() {
        _num->setConnected();
    }

    int rawFd() const {
        return _num->rawFd();
    }

    const SockNum::Ptr& sockNum() const {
        return _num;
    }

    SockNum::SockType type() {
        return _num->type();
    }

    const EventPoller::Ptr& getPoller() const {
        return _poller;
    }

private:
    SockNum::Ptr _num;
    EventPoller::Ptr _poller;
};

template<class Mtx = std::recursive_mutex>
class MutexWrapper {
public:
    MutexWrapper(bool enable) {
        _enable = enable;
    }

    ~MutexWrapper() = default;

    inline void lock() {
        if (_enable) {
            _mtx.lock();
        }
    }

    inline void unlock() {
        if (_enable) {
            _mtx.unlock();
        }
    }

private:
    bool _enable;
    Mtx _mtx;
};

class SockInfo {
public:
    SockInfo() = default;
    virtual ~SockInfo() = default;

    //获取本机ip  [AUTO-TRANSLATED:02d3901d]
    //Get local IP
    virtual std::string get_local_ip() = 0;
    //获取本机端口号  [AUTO-TRANSLATED:f883cf62]
    //Get local port number
    virtual uint16_t get_local_port() = 0;
    //获取对方ip  [AUTO-TRANSLATED:f042aa78]
    //Get peer IP
    virtual std::string get_peer_ip() = 0;
    //获取对方端口号  [AUTO-TRANSLATED:0d085eca]
    //Get the peer's port number
    virtual uint16_t get_peer_port() = 0;
    //获取标识符  [AUTO-TRANSLATED:e623608c]
    //Get the identifier
    virtual std::string getIdentifier() const { return ""; }
};

#define TraceP(ptr) TraceL << ptr->getIdentifier() << "(" << ptr->get_peer_ip() << ":" << ptr->get_peer_port() << ") "
#define DebugP(ptr) DebugL << ptr->getIdentifier() << "(" << ptr->get_peer_ip() << ":" << ptr->get_peer_port() << ") "
#define InfoP(ptr) InfoL << ptr->getIdentifier() << "(" << ptr->get_peer_ip() << ":" << ptr->get_peer_port() << ") "
#define WarnP(ptr) WarnL << ptr->getIdentifier() << "(" << ptr->get_peer_ip() << ":" << ptr->get_peer_port() << ") "
#define ErrorP(ptr) ErrorL << ptr->getIdentifier() << "(" << ptr->get_peer_ip() << ":" << ptr->get_peer_port() << ") "

//异步IO Socket对象，包括tcp客户端、服务器和udp套接字  [AUTO-TRANSLATED:8d4fc5c2]
//Asynchronous IO Socket object, including TCP client, server, and UDP socket
class Socket : public std::enable_shared_from_this<Socket>, public noncopyable, public SockInfo {
public:
    using Ptr = std::shared_ptr<Socket>;
    //接收数据回调  [AUTO-TRANSLATED:e3b7ff16]
    //Receive data callback
    using onReadCB = std::function<void(Buffer::Ptr &buf, struct sockaddr *addr, int addr_len)>;
    using onMultiReadCB = toolkit::function_safe<void(Buffer::Ptr *buf, struct sockaddr_storage *addr, size_t count)>;

    //发生错误回调  [AUTO-TRANSLATED:d6897b99]
    //Error callback
    using onErrCB = toolkit::function_safe<void(const SockException &err)>;
    //tcp监听接收到连接请求  [AUTO-TRANSLATED:c4e1b206]
    //TCP listen receives a connection request
    using onAcceptCB = toolkit::function_safe<void(Socket::Ptr &sock, std::shared_ptr<void> &complete)>;
    //socket发送缓存清空事件，返回true代表下次继续监听该事件，否则停止  [AUTO-TRANSLATED:2dd1c036]
    //Socket send buffer is cleared event, returns true to continue listening for the event next time, otherwise stops
    using onFlush = toolkit::function_safe<bool()>;
    //在接收到连接请求前，拦截Socket默认生成方式  [AUTO-TRANSLATED:2f07f268]
    //Intercept the default generation method of the Socket before receiving a connection request
    using onCreateSocket = toolkit::function_safe<Ptr(const EventPoller::Ptr &poller)>;
    //发送buffer成功与否回调  [AUTO-TRANSLATED:4db5efb8]
    //Send buffer success or failure callback
    using onSendResult = BufferList::SendResult;

    /**
     * 构造socket对象，尚未有实质操作
     * @param poller 绑定的poller线程
     * @param enable_mutex 是否启用互斥锁(接口是否线程安全)
     * Construct a socket object, no actual operation yet
     * @param poller The bound poller thread
     * @param enable_mutex Whether to enable the mutex (whether the interface is thread-safe)
     
     * [AUTO-TRANSLATED:39bd767a]
    */
    static Ptr createSocket(const EventPoller::Ptr &poller = nullptr, bool enable_mutex = true);
    ~Socket() override;

    /**
     * 创建tcp客户端并异步连接服务器
     * @param url 目标服务器ip或域名
     * @param port 目标服务器端口
     * @param con_cb 结果回调
     * @param timeout_sec 超时时间
     * @param local_ip 绑定本地网卡ip
     * @param local_port 绑定本地网卡端口号
     * Create a TCP client and connect to the server asynchronously
     * @param url Target server IP or domain name
     * @param port Target server port
     * @param con_cb Result callback
     * @param timeout_sec Timeout time
     * @param local_ip Local network card IP to bind
     * @param local_port Local network card port number to bind
     
     * [AUTO-TRANSLATED:4f6c0d3e]
     */
    void connect(const std::string &url, uint16_t port, const onErrCB &con_cb, float timeout_sec = 5, const std::string &local_ip = "::", uint16_t local_port = 0);

    /**
     * 创建tcp监听服务器
     * @param port 监听端口，0则随机
     * @param local_ip 监听的网卡ip
     * @param backlog tcp最大积压数
     * @return 是否成功
     * Create a TCP listening server
     * @param port Listening port, 0 for random
     * @param local_ip Network card IP to listen on
     * @param backlog Maximum TCP backlog
     * @return Whether successful
     
     * [AUTO-TRANSLATED:c90ff571]
     */
    bool listen(uint16_t port, const std::string &local_ip = "::", int backlog = 1024);

    /**
     * 创建udp套接字,udp是无连接的，所以可以作为服务器和客户端
     * @param port 绑定的端口为0则随机
     * @param local_ip 绑定的网卡ip
     * @return 是否成功
     * Create a UDP socket, UDP is connectionless, so it can be used as a server and client
     * @param port Port to bind, 0 for random
     * @param local_ip Network card IP to bind
     * @return Whether successful
     
     * [AUTO-TRANSLATED:e96342b5]
     */
    bool bindUdpSock(uint16_t port, const std::string &local_ip = "::", bool enable_reuse = true);

    /**
     * 包装外部fd，本对象负责close fd
     * 内部会设置fd为NoBlocked,NoSigpipe,CloExec
     * 其他设置需要自行使用SockUtil进行设置
     * Wrap an external file descriptor, this object is responsible for closing the file descriptor
     * Internally, the file descriptor will be set to NoBlocked, NoSigpipe, CloExec
     * Other settings need to be set manually using SockUtil
     
     * [AUTO-TRANSLATED:a72fd2ad]
     */
    bool fromSock(int fd, SockNum::SockType type);

    /**
     * 从另外一个Socket克隆
     * 目的是一个socket可以被多个poller对象监听，提高性能或实现Socket归属线程的迁移
     * @param other 原始的socket对象
     * @return 是否成功
     * Clone from another Socket
     * The purpose is to allow a socket to be listened to by multiple poller objects, improving performance or implementing socket migration between threads
     * @param other Original socket object
     * @return Whether successful
     
     * [AUTO-TRANSLATED:b3669f71]
     */
    bool cloneSocket(const Socket &other);

    ////////////设置事件回调////////////  [AUTO-TRANSLATED:0bfc62ce]
    //////////// Set event callbacks ////////////

    /**
     * 设置数据接收回调,tcp或udp客户端有效
     * @param cb 回调对象
     * Set data receive callback, valid for TCP or UDP clients
     * @param cb Callback object
     
     * [AUTO-TRANSLATED:d3f7ae8a]
     */
    void setOnRead(onReadCB cb);
    void setOnMultiRead(onMultiReadCB cb);

    /**
     * 设置异常事件(包括eof等)回调
     * @param cb 回调对象
     * Set exception event (including EOF) callback
     * @param cb Callback object
     
     * [AUTO-TRANSLATED:ffbea52f]
     */
    void setOnErr(onErrCB cb);

    /**
     * 设置tcp监听接收到连接回调
     * @param cb 回调对象
     * Set TCP listening receive connection callback
     * @param cb Callback object
     
     * [AUTO-TRANSLATED:cdcfdb9c]
     */
    void setOnAccept(onAcceptCB cb);

    /**
     * 设置socket写缓存清空事件回调
     * 通过该回调可以实现发送流控
     * @param cb 回调对象
     * Set socket write buffer clear event callback
     * This callback can be used to implement send flow control
     * @param cb Callback object
     
     * [AUTO-TRANSLATED:a5ef862d]
     */
    void setOnFlush(onFlush cb);

    /**
     * 设置accept时，socket构造事件回调
     * @param cb 回调
     * Set accept callback when socket is constructed
     * @param cb callback
     
     * [AUTO-TRANSLATED:d946409b]
     */
    void setOnBeforeAccept(onCreateSocket cb);

    /**
     * 设置发送buffer结果回调
     * @param cb 回调
     * Set send buffer result callback
     * @param cb callback
     
     * [AUTO-TRANSLATED:1edb77bb]
     */
    void setOnSendResult(onSendResult cb);

    ////////////发送数据相关接口////////////  [AUTO-TRANSLATED:c14ca1a7]
    ////////////Data sending related interfaces////////////

    /**
     * 发送数据指针
     * @param buf 数据指针
     * @param size 数据长度
     * @param addr 目标地址
     * @param addr_len 目标地址长度
     * @param try_flush 是否尝试写socket
     * @return -1代表失败(socket无效)，0代表数据长度为0，否则返回数据长度
     * Send data pointer
     * @param buf data pointer
     * @param size data length
     * @param addr target address
     * @param addr_len target address length
     * @param try_flush whether to try writing to the socket
     * @return -1 represents failure (invalid socket), 0 represents data length is 0, otherwise returns data length
     
     * [AUTO-TRANSLATED:718d6192]
     */
    ssize_t send(const char *buf, size_t size = 0, struct sockaddr *addr = nullptr, socklen_t addr_len = 0, bool try_flush = true);

    /**
     * 发送string
     * Send string
     
     * [AUTO-TRANSLATED:f9dfdfcf]
     */
    ssize_t send(std::string buf, struct sockaddr *addr = nullptr, socklen_t addr_len = 0, bool try_flush = true);

    /**
     * 发送Buffer对象，Socket对象发送数据的统一出口
     * socket对象发送数据的统一出口
     * Send Buffer object, unified exit for Socket object to send data
     * unified exit for Socket object to send data
     
     * [AUTO-TRANSLATED:5e69facd]
     */
    ssize_t send(Buffer::Ptr buf, struct sockaddr *addr = nullptr, socklen_t addr_len = 0, bool try_flush = true);

    /**
     * 尝试将所有数据写socket
     * @return -1代表失败(socket无效或者发送超时)，0代表成功?
     * Try to write all data to the socket
     * @return -1 represents failure (invalid socket or send timeout), 0 represents success?
     
     * [AUTO-TRANSLATED:8e975c68]
     */
    int flushAll();

    /**
     * 关闭socket且触发onErr回调，onErr回调将在poller线程中进行
     * @param err 错误原因
     * @return 是否成功触发onErr回调
     * Close the socket and trigger the onErr callback, the onErr callback will be executed in the poller thread
     * @param err error reason
     * @return whether the onErr callback is successfully triggered
     
     * [AUTO-TRANSLATED:366db327]
     */
    bool emitErr(const SockException &err) noexcept;

    /**
     * 关闭或开启数据接收
     * @param enabled 是否开启
     * Enable or disable data reception
     * @param enabled whether to enable
     
     * [AUTO-TRANSLATED:95cdec39]
     */
    void enableRecv(bool enabled);

    /**
     * 获取裸文件描述符，请勿进行close操作(因为Socket对象会管理其生命周期)
     * @return 文件描述符
     * Get the raw file descriptor, do not perform close operation (because the Socket object will manage its lifecycle)
     * @return file descriptor
     
     * [AUTO-TRANSLATED:75417922]
     */
    int rawFD() const;

    /**
     * tcp客户端是否处于连接状态
     * 支持Sock_TCP类型socket
     * Whether the TCP client is in a connected state
     * Supports Sock_TCP type socket
     
     * [AUTO-TRANSLATED:42c0c094]
     */
    bool alive() const;

    /**
     * 返回socket类型
     * Returns the socket type
     
     * [AUTO-TRANSLATED:2009a5d2]
     */
    SockNum::SockType sockType() const;

    /**
     * 设置发送超时主动断开时间;默认10秒
     * @param second 发送超时数据，单位秒
     * Sets the send timeout to disconnect actively; default 10 seconds
     * @param second Send timeout data, in seconds
     
     * [AUTO-TRANSLATED:49127ce8]
     */
    void setSendTimeOutSecond(uint32_t second);

    /**
     * 套接字是否忙，如果套接字写缓存已满则返回true
     * @return 套接字是否忙
     * Whether the socket is busy, if the socket write buffer is full, returns true
     * @return Whether the socket is busy
     
     * [AUTO-TRANSLATED:4b753c62]
     */
    bool isSocketBusy() const;

    /**
     * 获取poller线程对象
     * @return poller线程对象
     * Gets the poller thread object
     * @return poller thread object
     
     * [AUTO-TRANSLATED:cfc5d2c4]
     */
    const EventPoller::Ptr &getPoller() const;

    /**
     * 绑定udp 目标地址，后续发送时就不用再单独指定了
     * @param dst_addr 目标地址
     * @param addr_len 目标地址长度
     * @param soft_bind 是否软绑定，软绑定时不调用udp connect接口，只保存目标地址信息，发送时再传递到sendto函数
     * @return 是否成功
     * Binds the UDP target address, subsequent sends do not need to specify it separately
     * @param dst_addr Target address
     * @param addr_len Target address length
     * @param soft_bind Whether to soft bind, soft binding does not call the UDP connect interface, only saves the target address information, and sends it to the sendto function
     * @return Whether successful
     
     * [AUTO-TRANSLATED:946bfe2a]
     */
    bool bindPeerAddr(const struct sockaddr *dst_addr, socklen_t addr_len = 0, bool soft_bind = false);

    /**
     * 设置发送flags
     * @param flags 发送的flag
     * Sets the send flags
     * @param flags Send flags
     
     * [AUTO-TRANSLATED:2b11445c]
     */
    void setSendFlags(int flags = SOCKET_DEFAULE_FLAGS);

    /**
     * 关闭套接字
     * @param close_fd 是否关闭fd还是只移除io事件监听
     * Closes the socket
     * @param close_fd Whether to close the fd or only remove the IO event listener
     
     * [AUTO-TRANSLATED:db624fc6]
     */
    void closeSock(bool close_fd = true);

    /**
     * 获取发送缓存包个数(不是字节数)
     * Gets the number of packets in the send buffer (not the number of bytes)
     
     * [AUTO-TRANSLATED:2f853b18]
     */
    size_t getSendBufferCount();

    /**
     * 获取上次socket发送缓存清空至今的毫秒数,单位毫秒
     * Gets the number of milliseconds since the last socket send buffer was cleared, in milliseconds
     
     * [AUTO-TRANSLATED:567c2818]
     */
    uint64_t elapsedTimeAfterFlushed();

    /**
     * 获取接收速率，单位bytes/s
     * Get the receiving rate, in bytes/s
     
     * [AUTO-TRANSLATED:5de8aa1c]
     */
    size_t getRecvSpeed();

    /**
     * 获取发送速率，单位bytes/s
     * Get the sending rate, in bytes/s
     
     * [AUTO-TRANSLATED:96a2595d]
     */
    size_t getSendSpeed();

    /**
     * 获取接收总字节数
     * Get the total recv bytes

     * [AUTO-TRANSLATED:5de8aa1c]
     */
    size_t getRecvTotalBytes();

    /**
     * 获取发送总字节数
     * Get the total send bytes

     * [AUTO-TRANSLATED:5de8aa1c]
     */
    size_t getSendTotalBytes();

    ////////////SockInfo override////////////
    std::string get_local_ip() override;
    uint16_t get_local_port() override;
    std::string get_peer_ip() override;
    uint16_t get_peer_port() override;
    std::string getIdentifier() const override;

private:
    Socket(EventPoller::Ptr poller, bool enable_mutex = true);

    void setSock(SockNum::Ptr sock);
    int onAccept(const SockNum::Ptr &sock, int event) noexcept;
    ssize_t onRead(const SockNum::Ptr &sock, const SocketRecvBuffer::Ptr &buffer) noexcept;
    void onWriteAble(const SockNum::Ptr &sock);
    void onConnected(const SockNum::Ptr &sock, const onErrCB &cb);
    void onFlushed();
    void startWriteAbleEvent(const SockNum::Ptr &sock);
    void stopWriteAbleEvent(const SockNum::Ptr &sock);
    bool flushData(const SockNum::Ptr &sock, bool poller_thread);
    bool attachEvent(const SockNum::Ptr &sock);
    ssize_t send_l(Buffer::Ptr buf, bool is_buf_sock, bool try_flush = true);
    void connect_l(const std::string &url, uint16_t port, const onErrCB &con_cb_in, float timeout_sec, const std::string &local_ip, uint16_t local_port);
    bool fromSock_l(SockNum::Ptr sock);

private:
    // send socket时的flag  [AUTO-TRANSLATED:e364a1bf]
    //Flag for sending socket
    int _sock_flags = SOCKET_DEFAULE_FLAGS;
    // 最大发送缓存，单位毫秒，距上次发送缓存清空时间不能超过该参数  [AUTO-TRANSLATED:3bd6dba3]
    //Maximum send buffer, in milliseconds, the time since the last send buffer was cleared cannot exceed this parameter
    uint32_t _max_send_buffer_ms = SEND_TIME_OUT_SEC * 1000;
    // 控制是否接收监听socket可读事件，关闭后可用于流量控制  [AUTO-TRANSLATED:71de6ece]
    //Control whether to receive listen socket readable events, can be used for traffic control after closing
    std::atomic<bool> _enable_recv { true };
    // 标记该socket是否可写，socket写缓存满了就不可写  [AUTO-TRANSLATED:32392de2]
    //Mark whether the socket is writable, the socket write buffer is full and cannot be written
    std::atomic<bool> _sendable { true };
    // 是否已经触发err回调了  [AUTO-TRANSLATED:17ab8384]
    //Whether the err callback has been triggered
    bool _err_emit = false;
    // 是否启用网速统计  [AUTO-TRANSLATED:c0c0e8ee]
    //Whether to enable network speed statistics
    bool _enable_speed = false;
    // udp发送目标地址  [AUTO-TRANSLATED:cce2315a]
    //UDP send target address
    std::shared_ptr<struct sockaddr_storage> _udp_send_dst;

    // 接收速率统计  [AUTO-TRANSLATED:20dcd724]
    //Receiving rate statistics
    BytesSpeed _recv_speed;
    // 发送速率统计  [AUTO-TRANSLATED:eab3486a]
    //Send rate statistics
    BytesSpeed _send_speed;

    // tcp连接超时定时器  [AUTO-TRANSLATED:1b3e5fc4]
    //TCP connection timeout timer
    Timer::Ptr _con_timer;
    // tcp连接结果回调对象  [AUTO-TRANSLATED:4f1c366a]
    //TCP connection result callback object
    std::shared_ptr<void> _async_con_cb;

    // 记录上次发送缓存(包括socket写缓存、应用层缓存)清空的计时器  [AUTO-TRANSLATED:2c44d156]
    //Record the timer for the last send buffer (including socket write buffer and application layer buffer) cleared
    Ticker _send_flush_ticker;
    // socket fd的抽象类  [AUTO-TRANSLATED:31e4ea33]
    //Abstract class for socket fd
    SockFD::Ptr _sock_fd;
    // 本socket绑定的poller线程，事件触发于此线程  [AUTO-TRANSLATED:6f782513]
    //The poller thread bound to this socket, events are triggered in this thread
    EventPoller::Ptr _poller;
    // 跨线程访问_sock_fd时需要上锁  [AUTO-TRANSLATED:dc63f6c4]
    //Need to lock when accessing _sock_fd across threads
    mutable MutexWrapper<std::recursive_mutex> _mtx_sock_fd;

    // socket异常事件(比如说断开)  [AUTO-TRANSLATED:96c028e8]
    //Socket exception event (such as disconnection)
    onErrCB _on_err;
    // 收到数据事件  [AUTO-TRANSLATED:23946f9b]
    //Receive data event
    onMultiReadCB _on_multi_read;
    // socket缓存清空事件(可用于发送流速控制)  [AUTO-TRANSLATED:976b84ef]
    //Socket buffer cleared event (can be used for send flow control)
    onFlush _on_flush;
    // tcp监听收到accept请求事件  [AUTO-TRANSLATED:5fe01738]
    //TCP listener receives an accept request event
    onAcceptCB _on_accept;
    // tcp监听收到accept请求，自定义创建peer Socket事件(可以控制子Socket绑定到其他poller线程)  [AUTO-TRANSLATED:da85b845]
    //TCP listener receives an accept request, custom creation of peer Socket event (can control binding of child Socket to other poller threads)
    onCreateSocket _on_before_accept;
    // 设置上述回调函数的锁  [AUTO-TRANSLATED:302ca377]
    //Set the lock for the above callback function
    MutexWrapper<std::recursive_mutex> _mtx_event;

    // 一级发送缓存, socket可写时，会把一级缓存批量送入到二级缓存  [AUTO-TRANSLATED:26f1da58]
    //First-level send cache, when the socket is writable, it will batch the first-level cache into the second-level cache
    List<std::pair<Buffer::Ptr, bool>> _send_buf_waiting;
    // 一级发送缓存锁  [AUTO-TRANSLATED:9ec6c6a9]
    //First-level send cache lock
    MutexWrapper<std::recursive_mutex> _mtx_send_buf_waiting;
    // 二级发送缓存, socket可写时，会把二级缓存批量写入到socket  [AUTO-TRANSLATED:cc665665]
    //Second-level send cache, when the socket is writable, it will batch the second-level cache into the socket
    List<BufferList::Ptr> _send_buf_sending;
    // 二级发送缓存锁  [AUTO-TRANSLATED:306e3472]
    //Second-level send cache lock
    MutexWrapper<std::recursive_mutex> _mtx_send_buf_sending;
    // 发送buffer结果回调  [AUTO-TRANSLATED:1cac46fd]
    //Send buffer result callback
    BufferList::SendResult _send_result;
    // 对象个数统计  [AUTO-TRANSLATED:f4a012d0]
    //Object count statistics
    ObjectStatistic<Socket> _statistic;

    // 链接缓存地址,防止tcp reset 导致无法获取对端的地址  [AUTO-TRANSLATED:f8847463]
    //Connection cache address, to prevent TCP reset from causing the inability to obtain the peer's address
    struct sockaddr_storage _local_addr;
    struct sockaddr_storage _peer_addr;
};

class SockSender {
public:
    SockSender() = default;
    virtual ~SockSender() = default;
    virtual ssize_t send(Buffer::Ptr buf) = 0;
    virtual void shutdown(const SockException &ex = SockException(Err_shutdown, "self shutdown")) = 0;

    //发送char *  [AUTO-TRANSLATED:ab84aeb3]
    //Send char *
    SockSender &operator << (const char *buf);
    //发送字符串  [AUTO-TRANSLATED:3d678d0a]
    //Send string
    SockSender &operator << (std::string buf);
    //发送Buffer对象  [AUTO-TRANSLATED:8a6fb71c]
    //Send Buffer object
    SockSender &operator << (Buffer::Ptr buf);

    //发送其他类型是数据  [AUTO-TRANSLATED:86b0319a]
    //Send other types of data
    template<typename T>
    SockSender &operator << (T &&buf) {
        std::ostringstream ss;
        ss << std::forward<T>(buf);
        send(ss.str());
        return *this;
    }

    ssize_t send(std::string buf);
    ssize_t send(const char *buf, size_t size = 0);
};

//Socket对象的包装类  [AUTO-TRANSLATED:9d384814]
//Socket object wrapper class
class SocketHelper : public SockSender, public SockInfo, public TaskExecutorInterface, public std::enable_shared_from_this<SocketHelper> {
public:
    using Ptr = std::shared_ptr<SocketHelper>;
    SocketHelper(const Socket::Ptr &sock);
    ~SocketHelper() override = default;

    ///////////////////// Socket util std::functions /////////////////////
    /**
     * 获取poller线程
     * Get poller thread
     
     * [AUTO-TRANSLATED:bd1ed6dc]
     */
    const EventPoller::Ptr& getPoller() const;

    /**
     * 设置批量发送标记,用于提升性能
     * @param try_flush 批量发送标记
     * Set batch send flag, used to improve performance
     * @param try_flush Batch send flag
     
     * [AUTO-TRANSLATED:8c3f2ae1]
     */
    void setSendFlushFlag(bool try_flush);

    /**
     * 设置socket发送flags
     * @param flags socket发送flags
     * Set socket send flags
     * @param flags Socket send flags
     
     * [AUTO-TRANSLATED:d5d2eec9]
     */
    void setSendFlags(int flags);

    /**
     * 套接字是否忙，如果套接字写缓存已满则返回true
     * Whether the socket is busy, returns true if the socket write buffer is full
     
     * [AUTO-TRANSLATED:5c3cc85c]
     */
    bool isSocketBusy() const;

    /**
     * 设置Socket创建器，自定义Socket创建方式
     * @param cb 创建器
     * Set Socket creator, customize Socket creation method
     * @param cb Creator
     
     * [AUTO-TRANSLATED:df045ccf]
     */
    void setOnCreateSocket(Socket::onCreateSocket cb);

    /**
     * 创建socket对象
     * Create a socket object
     
     * [AUTO-TRANSLATED:260848b5]
     */
    Socket::Ptr createSocket();

    /**
     * 获取socket对象
     * Get the socket object
     
     * [AUTO-TRANSLATED:f737fb8d]
     */
    const Socket::Ptr &getSock() const;

    /**
     * 尝试将所有数据写socket
     * @return -1代表失败(socket无效或者发送超时)，0代表成功?
     * Try to write all data to the socket
     * @return -1 represents failure (invalid socket or send timeout), 0 represents success
     
     * [AUTO-TRANSLATED:8e975c68]
     */
    int flushAll();

    /**
     * 是否ssl加密
     * Whether SSL encryption is enabled
     
     * [AUTO-TRANSLATED:95b748f2]
     */
    virtual bool overSsl() const { return false; }

    ///////////////////// SockInfo override /////////////////////
    std::string get_local_ip() override;
    uint16_t get_local_port() override;
    std::string get_peer_ip() override;
    uint16_t get_peer_port() override;

    ///////////////////// TaskExecutorInterface override /////////////////////
    /**
     * 任务切换到所属poller线程执行
     * @param task 任务
     * @param may_sync 是否运行同步执行任务
     * Switch the task to the poller thread for execution
     * @param task The task to be executed
     * @param may_sync Whether to run the task synchronously
     
     * [AUTO-TRANSLATED:c0a93c6e]
     */
    Task::Ptr async(TaskIn task, bool may_sync = true) override;
    Task::Ptr async_first(TaskIn task, bool may_sync = true) override;

    ///////////////////// SockSender override /////////////////////

    /**
     * 使能 SockSender 其他未被重写的send重载函数
     * Enable other non-overridden send functions in SockSender
     
     * [AUTO-TRANSLATED:e6baa93a]
     */
    using SockSender::send;

    /**
     * 统一发送数据的出口
     * Unified data sending outlet
     
     * [AUTO-TRANSLATED:6a7a5178]
     */
    ssize_t send(Buffer::Ptr buf) override;

    /**
     * 触发onErr事件
     * Trigger the onErr event
     
     * [AUTO-TRANSLATED:b485450f]
     */
    void shutdown(const SockException &ex = SockException(Err_shutdown, "self shutdown")) override;

    /**
     * 线程安全的脱离 Server 并触发 onError 事件
     * @param ex 触发 onError 事件的原因
     * Safely detach from the Server and trigger the onError event in a thread-safe manner
     * @param ex The reason for triggering the onError event
     
     * [AUTO-TRANSLATED:739455d5]
     */
    void safeShutdown(const SockException &ex = SockException(Err_shutdown, "self shutdown"));

    ///////////////////// event functions /////////////////////
    /**
     * 接收数据入口
     * @param buf 数据，可以重复使用内存区,不可被缓存使用
     * Data receiving entry point
     * @param buf Data buffer, can be reused, and cannot be cached
     
     * [AUTO-TRANSLATED:9d498f56]
     */
    virtual void onRecv(const Buffer::Ptr &buf) = 0;

    /**
     * 收到 eof 或其他导致脱离 Server 事件的回调
     * 收到该事件时, 该对象一般将立即被销毁
     * @param err 原因
     * Callback received eof or other events that cause disconnection from Server
     * When this event is received, the object is generally destroyed immediately
     * @param err reason
     
     * [AUTO-TRANSLATED:a9349e0f]
     */
    virtual void onError(const SockException &err) = 0;

    /**
     * 数据全部发送完毕后回调
     * Callback after all data has been sent
     
     * [AUTO-TRANSLATED:8b2ba800]
     */
    virtual void onFlush() {}

    /**
     * 每隔一段时间触发, 用来做超时管理
     * Triggered at regular intervals, used for timeout management
     
     * [AUTO-TRANSLATED:af9e6c42]
     */
    virtual void onManager() = 0;

protected:
    void setPoller(const EventPoller::Ptr &poller);
    void setSock(const Socket::Ptr &sock);

private:
    bool _try_flush = true;
    Socket::Ptr _sock;
    EventPoller::Ptr _poller;
    Socket::onCreateSocket _on_create_socket;
};

}  // namespace toolkit
#endif /* NETWORK_SOCKET_H */
