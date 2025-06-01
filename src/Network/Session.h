﻿/*
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
#include "Socket.h"
#include "Util/util.h"
#include "Util/SSLBox.h"
#include "Kcp.h"

namespace toolkit {

// 会话, 用于存储一对客户端与服务端间的关系  [AUTO-TRANSLATED:d69736ea]
//Session, used to store the relationship between a client and a server
class Server;
class TcpSession;
class UdpSession;

class Session : public SocketHelper {
public:
    using Ptr = std::shared_ptr<Session>;

    Session(const Socket::Ptr &sock);
    ~Session() override = default;

    /**
     * 在创建 Session 后, Server 会把自身的配置参数通过该函数传递给 Session
     * @param server, 服务器对象
     * After creating a Session, the Server will pass its configuration parameters to the Session through this function
     * @param server, server object
     
     * [AUTO-TRANSLATED:5ce03e96]
     */
    virtual void attachServer(const Server &server) {}

    /**
     * 作为该 Session 的唯一标识符
     * @return 唯一标识符
     * As the unique identifier of this Session
     * @return unique identifier
     
     * [AUTO-TRANSLATED:3b046f26]
     */
    std::string getIdentifier() const override;

private:
    mutable std::string _id;
    std::unique_ptr<toolkit::ObjectStatistic<toolkit::TcpSession> > _statistic_tcp;
    std::unique_ptr<toolkit::ObjectStatistic<toolkit::UdpSession> > _statistic_udp;
};

// 通过该模板可以让TCP服务器快速支持TLS  [AUTO-TRANSLATED:fea218e6]
//This template allows the TCP server to quickly support TLS
template <typename SessionType>
class SessionWithSSL : public SessionType {
public:
    template <typename... ArgsType>
    SessionWithSSL(ArgsType &&...args)
        : SessionType(std::forward<ArgsType>(args)...) {
        _ssl_box.setOnEncData([&](const Buffer::Ptr &buf) { public_send(buf); });
        _ssl_box.setOnDecData([&](const Buffer::Ptr &buf) { public_onRecv(buf); });
    }

    ~SessionWithSSL() override { _ssl_box.flush(); }

    void onRecv(const Buffer::Ptr &buf) override { _ssl_box.onRecv(buf); }

    // 添加public_onRecv和public_send函数是解决较低版本gcc一个lambad中不能访问protected或private方法的bug  [AUTO-TRANSLATED:7b16e05b]
    //Adding public_onRecv and public_send functions is to solve a bug in lower versions of gcc where a lambda cannot access protected or private methods
    inline void public_onRecv(const Buffer::Ptr &buf) { SessionType::onRecv(buf); }
    inline void public_send(const Buffer::Ptr &buf) { SessionType::send(buf); }

    bool overSsl() const override { return true; }

protected:
    ssize_t send(Buffer::Ptr buf) override {
        auto size = buf->size();
        _ssl_box.onSend(std::move(buf));
        return size;
    }

private:
    SSL_Box _ssl_box;
};

// 通过该模板可以让UDP服务器快速支持KCP
template <typename SessionType>
class SessionWithKCP : public SessionType {
public:
    template <typename... ArgsType>
    SessionWithKCP(ArgsType &&...args)
        : SessionType(std::forward<ArgsType>(args)...) {
        _kcp_box = std::make_shared<KcpTransport>(true);
        _kcp_box->setOnWrite([&](const Buffer::Ptr &buf) { public_send(buf); });
        _kcp_box->setOnRead([&](const Buffer::Ptr &buf) { public_onRecv(buf); });
        _kcp_box->setOnErr([&](const SockException &ex) { public_onErr(ex); });
    }

    ~SessionWithKCP() override { }

    void onRecv(const Buffer::Ptr &buf) override { _kcp_box->input(buf); }

    inline void public_onRecv(const Buffer::Ptr &buf) { SessionType::onRecv(buf); }
    inline void public_send(const Buffer::Ptr &buf) { SessionType::send(buf); }
    inline void public_onErr(const SockException &ex) { SessionType::onError(ex); }

protected:
    ssize_t send(Buffer::Ptr buf) override {
        return _kcp_box->send(std::move(buf));
    }

private:
    KcpTransport::Ptr _kcp_box;
};

} // namespace toolkit

#endif // ZLTOOLKIT_SESSION_H
