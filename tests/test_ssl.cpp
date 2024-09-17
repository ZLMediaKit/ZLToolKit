/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
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
    //初始化设置日志  [AUTO-TRANSLATED:f8d72b7b]
    // Initialize log settings
    Logger::Instance().add(std::make_shared<ConsoleChannel> ());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    //加载证书，证书包含公钥和私钥  [AUTO-TRANSLATED:fce78641]
    // Load certificate, certificate contains public key and private key
    SSL_Initor::Instance().loadCertificate((exeDir() + "ssl.p12").data());
    SSL_Initor::Instance().trustCertificate((exeDir() + "ssl.p12").data());
    SSL_Initor::Instance().ignoreInvalidCertificate(false);

    //定义客户端和服务端  [AUTO-TRANSLATED:d419f035]
    // Define client and server
    SSL_Box client(false), server(true);

    //设置客户端解密输出回调  [AUTO-TRANSLATED:b98ceb19]
    // Set client decryption output callback
    client.setOnDecData([&](const Buffer::Ptr &buffer) {
        //打印来自服务端数据解密后的明文  [AUTO-TRANSLATED:c672d9f5]
        // Print plaintext from server after decryption
        InfoL << "client recv:" << buffer->toString();
    });

    //设置客户端加密输出回调  [AUTO-TRANSLATED:e69a01e4]
    // Set client encryption output callback
    client.setOnEncData([&](const Buffer::Ptr &buffer) {
        //把客户端加密后的密文发送给服务端  [AUTO-TRANSLATED:eb54076a]
        // Send encrypted ciphertext from client to server
        server.onRecv(buffer);
    });

    //设置服务端解密输出回调  [AUTO-TRANSLATED:79eb87c8]
    // Set server decryption output callback
    server.setOnDecData([&](const Buffer::Ptr &buffer) {
        //打印来自客户端数据解密后的明文  [AUTO-TRANSLATED:71ba8425]
        // Print plaintext from client after decryption
        InfoL << "server recv:" << buffer->toString();
        //把数据回显给客户端  [AUTO-TRANSLATED:cb8fd00a]
        // Echo data back to client
        server.onSend(buffer);
    });

    //设置服务端加密输出回调  [AUTO-TRANSLATED:b39c8f28]
    // Set server-side encryption output callback
    server.setOnEncData([&](const Buffer::Ptr &buffer) {
        //把加密的回显信息回复给客户端;  [AUTO-TRANSLATED:cd3754ed]
        // Return the encrypted echo information to the client;
        client.onRecv(buffer);
    });

    InfoL << "请输入字符开始测试,输入quit停止测试:" << endl;

    string input;
    while (true) {
        std::cin >> input;
        if (input == "quit") {
            break;
        }
        //把明文数据输入给客户端  [AUTO-TRANSLATED:0c10359d]
        // Input plaintext data to the client
        client.onSend(std::make_shared<BufferString>(std::move(input)));
    }
    return 0;
}
