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
#include "Util/util.h"
#include "Util/RingBuffer.h"
#include "Thread/threadgroup.h"
#include <list>

using namespace std;
using namespace toolkit;

//环形缓存写线程退出标记  [AUTO-TRANSLATED:ceeb4c96]
// Ring buffer write thread exit flag
bool g_bExitWrite = false;

//一个30个string对象的环形缓存  [AUTO-TRANSLATED:fee6a014]
// A ring buffer of 30 string objects
RingBuffer<string>::Ptr g_ringBuf(new RingBuffer<string>(30));

//写事件回调函数  [AUTO-TRANSLATED:19f6b6fa]
// Write event callback function
void onReadEvent(const string &str){
    //读事件模式性  [AUTO-TRANSLATED:12cdd3a8]
    // Read event mode
    DebugL << str;
}

//环形缓存销毁事件  [AUTO-TRANSLATED:7da356eb]
// Ring buffer destruction event
void onDetachEvent(){
    WarnL;
}

//写环形缓存任务  [AUTO-TRANSLATED:1467fea5]
// Write ring buffer task
void doWrite(){
    int i = 0;
    while(!g_bExitWrite){
        //每隔100ms写一个数据到环形缓存  [AUTO-TRANSLATED:aedec620]
        // Write data to the ring buffer every 100ms
        g_ringBuf->write(to_string(++i),true);
        usleep(100 * 1000);
    }

}
int main() {
    //初始化日志  [AUTO-TRANSLATED:371bb4e5]
    // Initialize log
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    auto poller = EventPollerPool::Instance().getPoller();
    RingBuffer<string>::RingReader::Ptr ringReader;
    poller->sync([&](){
        //从环形缓存获取一个读取器  [AUTO-TRANSLATED:c61b1c37]
        // Get a reader from the ring buffer
        ringReader = g_ringBuf->attach(poller);

        //设置读取事件  [AUTO-TRANSLATED:6d9e7c68]
        // Set read event
        ringReader->setReadCB([](const string &pkt){
            onReadEvent(pkt);
        });

        //设置环形缓存销毁事件  [AUTO-TRANSLATED:8fc24dc3]
        // Set ring buffer destruction event
        ringReader->setDetachCB([](){
            onDetachEvent();
        });
    });


    thread_group group;
    //写线程  [AUTO-TRANSLATED:f91ceacd]
    // Write thread
    group.create_thread([](){
        doWrite();
    });

    //测试3秒钟  [AUTO-TRANSLATED:942e7c08]
    // Test for 3 seconds
    sleep(3);

    //通知写线程退出  [AUTO-TRANSLATED:b2b2a2af]
    // Notify write thread to exit
    g_bExitWrite = true;
    //等待写线程退出  [AUTO-TRANSLATED:8150eaf3]
    // Wait for write thread to exit
    group.join_all();

    //释放环形缓冲，此时异步触发Detach事件  [AUTO-TRANSLATED:3aaaa2f4]
    // Release ring buffer, triggering Detach event asynchronously
    g_ringBuf.reset();
    //等待异步触发Detach事件  [AUTO-TRANSLATED:a6ea0f45]
    // Wait for asynchronous Detach event trigger
    sleep(1);
    //消除对EventPoller对象的引用  [AUTO-TRANSLATED:4f22453d]
    // Remove reference to EventPoller object
    ringReader.reset();
    sleep(1);
    return 0;
}











