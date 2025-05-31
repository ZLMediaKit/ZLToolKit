/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */
//模拟丢包率
//#清空规则
//tc qdisc del lo root
//#设置lo rtt 60ms, 丢包率 30%
//tc qdisc add lo root handle 1:htb
//tc class add dev lo parent 1: classid 1:1 htb rate 1000mbit
//tc qdisc add dev lo parent 1:1 delay 30ms loss 30%
//#过滤端口9000
//tc filter add dev lo protocol ip parent 1:0 u32 match ip dport 9000 0xffff flowid 1:1

#include <csignal>
#include <iostream>
#include "Util/logger.h"
#include "Util/Byte.hpp"
#include "Util/util.h"
#include "Network/UdpClient.h"
using namespace std;
using namespace toolkit;

static const size_t msg_len = 128; //date len 4 * 128 = 512
static const uint32_t tick_limit = 100* (1024 * 2); //100MB
static uint64_t clock_time;

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
    virtual void onRecvFrom(const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) override {
        // if (!addr) {
        //     DebugL << " recvfrom ip: " << SockUtil::inet_ntoa(addr) << ", port: " << SockUtil::inet_port(addr);
        // }
        // TraceL << hexdump(buf->data(), buf->size());
        //
        if (buf->size() != sizeof(uint32_t) * msg_len) {
            // WarnL << "recv msg mismatch";
            return;
        }

        auto tick = Byte::Get4Bytes((const uint8_t*)buf->data(), 0);
        if (_nTick != tick) {
            _nTick = tick + 1;
            // WarnL << "recv tick: " << tick << " mismatch nTick: " << _nTick;
            return;
        }

        _nTick++;
        _nHit++;
        if (tick > tick_limit - 100 && !_report) {
            _report = true;
            auto now = getCurrentMillisecond();
            InfoL << "starttime: " << clock_time 
                << "ms, endtime: " << now 
                << "ms, usetime: " << now - clock_time
                << "ms, " << ((uint64_t)(tick + 1 - _nHit) * 100 / (tick + 1))
                << "% loss";
        }
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
    uint32_t _nHit = 0;
    bool _report = false;
};

int main() {
    // 设置日志系统  [AUTO-TRANSLATED:45646031]
    // Set up the logging system
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    UdpClientWithKcp<TestClient>::Ptr client(new UdpClientWithKcp<TestClient>());//必须使用智能指针
    client->setInterval(10);
    client->setDelayMode(KcpTransport::DelayMode::DELAY_MODE_NO_DELAY);
    client->setFastResend(2);
    client->setWndSize(1024, 1024);
    client->setNoCwnd(true);
    // client->setRxMinrto(10);

    client->startConnect("127.0.0.1", 9000);//连接服务器
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
