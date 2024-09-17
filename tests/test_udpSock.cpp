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
#include "Util/util.h"
#include "Util/logger.h"
#include "Network/Socket.h"

using namespace std;
using namespace toolkit;

//主线程退出标志  [AUTO-TRANSLATED:4465f04c]
// Main thread exit flag
bool exitProgram = false;

//赋值struct sockaddr  [AUTO-TRANSLATED:07f9df9d]
// Assign struct sockaddr
void makeAddr(struct sockaddr_storage *out,const char *ip,uint16_t port){
    *out = SockUtil::make_sockaddr(ip, port);
}

//获取struct sockaddr的IP字符串  [AUTO-TRANSLATED:651562f1]
// Get IP string from struct sockaddr
string getIP(struct sockaddr *addr){
    return SockUtil::inet_ntoa(addr);
}

int main() {
    //设置程序退出信号处理函数  [AUTO-TRANSLATED:419fb1c3]
    // Set program exit signal handling function
    signal(SIGINT, [](int){exitProgram = true;});
    //设置日志系统  [AUTO-TRANSLATED:ad15b8d6]
    // Set up logging system
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    Socket::Ptr sockRecv = Socket::createSocket();//创建一个UDP数据接收端口
    Socket::Ptr sockSend = Socket::createSocket();//创建一个UDP数据发送端口
    sockRecv->bindUdpSock(9001);//接收UDP绑定9001端口
    sockSend->bindUdpSock(0, "0.0.0.0");//发送UDP随机端口

    sockRecv->setOnRead([](const Buffer::Ptr &buf, struct sockaddr *addr , int){
        //接收到数据回调  [AUTO-TRANSLATED:1cd064ad]
        // Data received callback
        DebugL << "recv data form " << getIP(addr) << ":" << buf->data();
    });

    struct sockaddr_storage addrDst;
    makeAddr(&addrDst,"127.0.0.1",9001);//UDP数据发送地址
//	sockSend->bindPeerAddr(&addrDst);
    int i = 0;
    while(!exitProgram){
        //每隔一秒往对方发送数据  [AUTO-TRANSLATED:d70ac05f]
        // Send data to the other side every second
        sockSend->send(to_string(i++), (struct sockaddr *)&addrDst);
        sleep(1);
    }
    return 0;
}





