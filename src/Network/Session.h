/*
 * Copyright (c) 2021 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLTOOLKIT_SESSION_H
#define ZLTOOLKIT_SESSION_H

#include <memory>
#include "Util/util.h"
#include "Socket.h"

namespace toolkit {

// 会话, 用于存储一对客户端与服务端间的关系
class Server;

class Session : public std::enable_shared_from_this<Session>, public SocketHelper {
public:
    using Ptr = std::shared_ptr<Session>;

    Session(const Socket::Ptr &sock);
    ~Session() override;

    /**
     * 接收数据入口
     * @param buf 数据，可以重复使用内存区,不可被缓存使用
     */
    virtual void onRecv(const Buffer::Ptr &buf) = 0;

    /**
     * 收到 eof 或其他导致脱离 Server 事件的回调
     * 收到该事件时, 该对象一般将立即被销毁
     * @param err 原因
     */
    virtual void onError(const SockException &err) = 0;

    /**
     * 每隔一段时间触发, 用来做超时管理
     */
    virtual void onManager() = 0;

    /**
     * 在创建 Session 后, Server 会把自身的配置参数通过该函数传递给 Session
     * @param server, 服务器对象
     */
    virtual void attachServer(const Server &server) {}

    /**
     * 作为该 Session 的唯一标识符
     * @return 唯一标识符
     */
    std::string getIdentifier() const override;

    /**
     * 线程安全的脱离 Server 并触发 onError 事件
     * @param ex 触发 onError 事件的原因
     */
    void safeShutdown(const SockException &ex = SockException(Err_shutdown, "self shutdown"));

private:
    mutable std::string _id;
    // 对象个数统计
    ObjectStatistic<Session> _statistic;
};

//TCP服务器连接对象，一个tcp连接对应一个TcpSession对象
class TcpSession : public Session {
public:
    using Ptr = std::shared_ptr<TcpSession>;

    TcpSession(const Socket::Ptr &sock) : Session(sock) {}
    ~TcpSession() override = default;

    Ptr shared_from_this() {
        return std::static_pointer_cast<TcpSession>(Session::shared_from_this());
    }

private:
    // 对象个数统计
    ObjectStatistic<TcpSession> _statistic;
};

//UDP服务器连接对象，一个udp peer对应一个UdpSession对象
class UdpSession : public Session {
public:
    using Ptr = std::shared_ptr<UdpSession>;

    UdpSession(const Socket::Ptr &sock) : Session(sock) {}
    ~UdpSession() override = default;

    Ptr shared_from_this() {
        return std::static_pointer_cast<UdpSession>(Session::shared_from_this());
    }

private:
    // 对象个数统计
    ObjectStatistic<UdpSession> _statistic;
};

} // namespace toolkit

#endif // ZLTOOLKIT_SESSION_H