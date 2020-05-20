/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <signal.h>
#include <iostream>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Network/TcpServer.h"
#include "Network/TcpSession.h"

using namespace std;
using namespace toolkit;

class EchoSession: public TcpSession {
public:
    EchoSession(const Socket::Ptr &sock) :
            TcpSession(sock) {
        DebugL;
    }
    ~EchoSession() {
        DebugL;
    }
    virtual void onRecv(const Buffer::Ptr &buf) override{
        //处理客户端发送过来的数据
        TraceL << buf->data() <<  " from port:" << get_local_port();
        send(buf);
    }
    virtual void onError(const SockException &err) override{
        //客户端断开连接或其他原因导致该对象脱离TCPServer管理
        WarnL << err.what();
    }
    virtual void onManager() override{
        //定时管理该对象，譬如会话超时检查
        DebugL;
    }

private:
    Ticker _ticker;
};


int main() {
    //初始化日志模块
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    //加载证书，证书包含公钥和私钥
    SSL_Initor::Instance().loadCertificate((exeDir() + "ssl.p12").data());
    SSL_Initor::Instance().trustCertificate((exeDir() + "ssl.p12").data());
    SSL_Initor::Instance().ignoreInvalidCertificate(false);

    TcpServer::Ptr server(new TcpServer());
    server->start<EchoSession>(9000);//监听9000端口

    TcpServer::Ptr serverSSL(new TcpServer());
    serverSSL->start<TcpSessionWithSSL<EchoSession> >(9001);//监听9001端口

    //退出程序事件处理
    static semaphore sem;
    signal(SIGINT, [](int) { sem.post(); });// 设置退出信号
    sem.wait();
    return 0;
}