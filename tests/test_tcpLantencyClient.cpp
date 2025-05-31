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
#include "Util/Byte.hpp"
#include "Util/util.h"
#include "Network/TcpClient.h"
using namespace std;
using namespace toolkit;

static const size_t msg_len = 128; //value shoule >= 2, date len 4 * 128 = 512 
static const uint32_t tick_limit = 100 * (1024 * 2); //100MB
static uint64_t clock_time;
static semaphore connect_sem;

class TestClient: public TcpClient {
public:
    using Ptr = std::shared_ptr<TestClient>;
    TestClient():TcpClient() {
        DebugL;
    }
    ~TestClient(){
        DebugL;
    }
protected:
    virtual void onConnect(const SockException &ex) override{
        //连接结果事件  [AUTO-TRANSLATED:46887902]
        // Connection established event
        InfoL << (ex ?  ex.what() : "success");
        connect_sem.post();
    }

    virtual void onRecv(const Buffer::Ptr &buf) override {
#if 0
        TraceL << hexdump(buf->data(), buf->size());
#endif
        _recv_len += buf->size();

        if (buf->size() >= 4) {
            auto tick = Byte::Get4Bytes((const uint8_t*)buf->data(), buf->size() - 4);
            if (tick > tick_limit - 100 && !_report) {
                size_t all_len = 4 * msg_len * (tick + 1);
                _report = true;
                auto now = getCurrentMillisecond();
                InfoL << "starttime: " << clock_time 
                    << "ms, endtime: " << now 
                    << "ms, usetime: " << now - clock_time
                    << "ms, " << ((uint64_t)(all_len - _recv_len) * 100 / all_len)
                    << "% loss";
            }
        }
    }
    virtual void onFlush() override{
        //发送阻塞后，缓存清空事件  [AUTO-TRANSLATED:46e8bca0]
        // Send blocked, cache cleared event
        DebugL;
    }
    virtual void onError(const SockException &ex) override{
        //断开连接事件，一般是EOF  [AUTO-TRANSLATED:7359fecf]
        // Disconnected event, usually EOF
        WarnL << ex.what();
    }

    virtual void onManager() override{

    }

private:
    uint32_t _nTick = 0;
    size_t _recv_len = 0;
    bool _report = false;
};

int main() {
    // 设置日志系统  [AUTO-TRANSLATED:45646031]
    // Set up the logging system
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    TestClient::Ptr client(new TestClient());//必须使用智能指针
    client->startConnect("127.0.0.1", 9000);//连接服务器
    client->setSendFlushFlag(true);
    connect_sem.wait();

    uint32_t tick = 0;
    while (tick <= tick_limit) {
        auto buf = BufferRaw::create(4 * msg_len);
        buf->setSize(4 * msg_len);
        for (int i = 0; i < msg_len; i++) {
            Byte::Set4Bytes((uint8_t*)buf->data(), 4 * i, tick);
        }
        // TraceL << hexdump(buf->data(), buf->size());
        client->send(buf);
        tick++;
    }

    auto now = getCurrentMillisecond();
    InfoL << "starttime: " << clock_time 
        << "ms, sendtime: " << now 
        << "ms, usetime: " << now - clock_time
        << "ms";

    //退出程序事件处理  [AUTO-TRANSLATED:80065cb7]
    // Exit program event handling
    static semaphore sem;
    signal(SIGINT, [](int) { sem.post(); });// 设置退出信号
    sem.wait();
    return 0;
}
