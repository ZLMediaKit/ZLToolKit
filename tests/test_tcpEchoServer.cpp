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
#include "Network/TcpServer.h"
#include "Network/Session.h"

using namespace std;
using namespace toolkit;

class EchoSession: public Session {
public:
    EchoSession(const Socket::Ptr &sock) :
            Session(sock) {
        DebugL;
    }
    ~EchoSession() {
        DebugL;
    }
    virtual void onRecv(const Buffer::Ptr &buf) override{
        //处理客户端发送过来的数据  [AUTO-TRANSLATED:c095b82e]
        // Handle data sent from the client
        TraceL << buf->data() <<  " from port:" << get_local_port();
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
        DebugL;
    }

private:
    Ticker _ticker;
};


int main() {
    //初始化日志模块  [AUTO-TRANSLATED:fd9321b2]
    // Initialize the log module
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    //加载证书，证书包含公钥和私钥  [AUTO-TRANSLATED:fce78641]
    // Load the certificate, the certificate contains the public key and private key
    SSL_Initor::Instance().loadCertificate((exeDir() + "ssl.p12").data());
    SSL_Initor::Instance().trustCertificate((exeDir() + "ssl.p12").data());
    SSL_Initor::Instance().ignoreInvalidCertificate(false);

    TcpServer::Ptr server(new TcpServer());
    server->start<EchoSession>(9000);//监听9000端口

    TcpServer::Ptr serverSSL(new TcpServer());
    serverSSL->start<SessionWithSSL<EchoSession> >(9001);//监听9001端口

    //退出程序事件处理  [AUTO-TRANSLATED:80065cb7]
    // Exit program event handling
    static semaphore sem;
    signal(SIGINT, [](int) { sem.post(); });// 设置退出信号
    sem.wait();
    return 0;
}