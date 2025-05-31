/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <csignal>
#include <iostream>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Util/Byte.hpp"
#include "Network/UdpServer.h"
#include "Network/Session.h"

using namespace std;
using namespace toolkit;

static const size_t msg_len = 128; //date len 4 * 128 = 512
static const uint32_t tick_limit = 1* (1024 * 2); //MB
static uint64_t clock_time;

class EchoSession: public Session {
public:
    EchoSession(const Socket::Ptr &sock) :
            Session(sock) {
        DebugL;
    }
    ~EchoSession() {
        DebugL;
    }
    virtual void onRecv(const Buffer::Ptr &buf) override {
        //处理客户端发送过来的数据  [AUTO-TRANSLATED:c095b82e]
        // Handle data sent from the client
        // TraceL << hexdump(buf->data(), buf->size()) <<  " from port:" << get_local_port();
        send(buf);
    }
    virtual void onError(const SockException &err) override{
        //客户端断开连接或其他原因导致该对象脱离TCPServer管理  [AUTO-TRANSLATED:6b958a7b]
        // Client disconnects or other reasons cause the object to be removed from TCPServer management
        WarnL << err;
    }
    virtual void onManager() override{
        //定时管理该对象，譬如会话超时检查  [AUTO-TRANSLATED:2caa54f6]
        // Periodically manage the object, such as session timeout check
        // DebugL;
    }

private:
    uint32_t _nTick = 0;
};

//通过模板全特化实现对指定会话拥塞参数的调整
namespace toolkit {
template <>
class SessionWithKCP<EchoSession> : public EchoSession {
public:
    template <typename... ArgsType>
    SessionWithKCP(ArgsType &&...args)
        : EchoSession(std::forward<ArgsType>(args)...) {
        _kcp_box = std::make_shared<KcpTransport>(true);
        _kcp_box->setOnWrite([&](const Buffer::Ptr &buf) { public_send(buf); });
        _kcp_box->setOnRead([&](const Buffer::Ptr &buf) { public_onRecv(buf); });
        _kcp_box->setOnErr([&](const SockException &ex) { public_onErr(ex); });
        _kcp_box->setInterval(10);
        _kcp_box->setDelayMode(KcpTransport::DelayMode::DELAY_MODE_NO_DELAY);
        _kcp_box->setFastResend(2);
        _kcp_box->setWndSize(1024, 1024);
        _kcp_box->setNoCwnd(true);
        // _kcp_box->setRxMinrto(10);
    }

    ~SessionWithKCP() override { }

    void onRecv(const Buffer::Ptr &buf) override { _kcp_box->input(buf); }

    inline void public_onRecv(const Buffer::Ptr &buf) { EchoSession::onRecv(buf); }
    inline void public_send(const Buffer::Ptr &buf) { EchoSession::send(buf); }
    inline void public_onErr(const SockException &ex) { EchoSession::onError(ex); }

protected:
    ssize_t send(Buffer::Ptr buf) override {
        return _kcp_box->send(std::move(buf));
    }

private:
    KcpTransport::Ptr _kcp_box;
};
}

int main() {
    //初始化日志模块  [AUTO-TRANSLATED:fd9321b2]
    // Initialize the log module
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    UdpServer::Ptr server(new UdpServer());
    server->start<SessionWithKCP<EchoSession> >(9000);//监听9000端口

    //退出程序事件处理  [AUTO-TRANSLATED:80065cb7]
    // Exit program event handling
    static semaphore sem;
    signal(SIGINT, [](int) { sem.post(); });// 设置退出信号
    sem.wait();
    return 0;
}
