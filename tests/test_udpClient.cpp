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
#include "Util/logger.h"
#include "Network/UdpClient.h"
using namespace std;
using namespace toolkit;

class TestClient: public UdpClient {
public:
    using Ptr = std::shared_ptr<TestClient>;
    TestClient():UdpClient() {
        DebugL;
    }
    ~TestClient(){
        DebugL;
    }
protected:
    virtual void onRecvFrom(const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) override{
        if (!addr) {
            DebugL << " recvfrom ip: " << SockUtil::inet_ntoa(addr) << ", port: " << SockUtil::inet_port(addr);
        }
        TraceL << hexdump(buf->data(), buf->size());
    }

    virtual void onError(const SockException &ex) override{
        //断开连接事件，一般是EOF  [AUTO-TRANSLATED:7359fecf]
        // Disconnected event, usually EOF
        WarnL << ex.what();
    }

    virtual void onManager() override{
        //定时发送数据到服务器  [AUTO-TRANSLATED:688c9148]
        // Periodically send data to the server
        std::string msg  =(StrPrinter << _nTick++ << " "
                           << 3.14 << " "
                           << "string" << " "
                           << "[BufferRaw]\0");
        (*this) << msg;
    }

private:
    int _nTick = 0;
};


int main() {
    // 设置日志系统  [AUTO-TRANSLATED:45646031]
    // Set up the logging system
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    TestClient::Ptr client(new TestClient());//必须使用智能指针
    client->startConnect("127.0.0.1", 9000);//连接服务器

    //退出程序事件处理  [AUTO-TRANSLATED:80065cb7]
    // Exit program event handling
    static semaphore sem;
    signal(SIGINT, [](int) { sem.post(); });// 设置退出信号
    sem.wait();
    return 0;
}
