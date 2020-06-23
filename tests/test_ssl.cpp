/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include "Util/logger.h"
#include "Util/util.h"
#include "Util/SSLBox.h"
using namespace std;
using namespace toolkit;

int main(int argc,char *argv[]) {
    //初始化设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel> ());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    //加载证书，证书包含公钥和私钥
    SSL_Initor::Instance().loadCertificate((exeDir() + "ssl.p12").data());
    SSL_Initor::Instance().trustCertificate((exeDir() + "ssl.p12").data());
    SSL_Initor::Instance().ignoreInvalidCertificate(false);

    //定义客户端和服务端
    SSL_Box client(false), server(true);

    //设置客户端解密输出回调
    client.setOnDecData([&](const Buffer::Ptr &buffer) {
        //打印来自服务端数据解密后的明文
        InfoL << "client recv:" << buffer->toString();
    });

    //设置客户端加密输出回调
    client.setOnEncData([&](const Buffer::Ptr &buffer) {
        //把客户端加密后的密文发送给服务端
        server.onRecv(buffer);
    });

    //设置服务端解密输出回调
    server.setOnDecData([&](const Buffer::Ptr &buffer) {
        //打印来自客户端数据解密后的明文
        InfoL << "server recv:" << buffer->toString();
        //把数据回显给客户端
        server.onSend(buffer);
    });

    //设置服务端加密输出回调
    server.setOnEncData([&](const Buffer::Ptr &buffer) {
        //把加密的回显信息回复给客户端;
        client.onRecv(buffer);
    });

    InfoL << "请输入字符开始测试,输入quit停止测试:" << endl;

    string input;
    while (true) {
        std::cin >> input;
        if (input == "quit") {
            break;
        }
        //把明文数据输入给客户端
        client.onSend(std::make_shared<BufferString>(std::move(input)));
    }
    return 0;
}
