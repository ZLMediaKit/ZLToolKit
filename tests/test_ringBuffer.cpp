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
#include "Util/logger.h"
#include "Util/util.h"
#include "Util/RingBuffer.h"
#include "Thread/threadgroup.h"
#include <list>

using namespace std;
using namespace toolkit;

//环形缓存写线程退出标记
bool g_bExitWrite = false;

//一个30个string对象的环形缓存
RingBuffer<string>::Ptr g_ringBuf(new RingBuffer<string>(30));

//写事件回调函数
void onReadEvent(const string &str){
    //读事件模式性
    DebugL << str;
}

//环形缓存销毁事件
void onDetachEvent(){
    WarnL;
}

//写环形缓存任务
void doWrite(){
    int i = 0;
    while(!g_bExitWrite){
        //每隔100ms写一个数据到环形缓存
        g_ringBuf->write(to_string(++i),true);
        usleep(100 * 1000);
    }

}
int main() {
    //初始化日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    auto poller = EventPollerPool::Instance().getPoller();
    RingBuffer<string>::RingReader::Ptr ringReader;
    poller->sync([&](){
        //从环形缓存获取一个读取器
        ringReader = g_ringBuf->attach(poller);

        //设置读取事件
        ringReader->setReadCB([](const string &pkt){
            onReadEvent(pkt);
        });

        //设置环形缓存销毁事件
        ringReader->setDetachCB([](){
            onDetachEvent();
        });
    });


    thread_group group;
    //写线程
    group.create_thread([](){
        doWrite();
    });

    //测试3秒钟
    sleep(3);

    //通知写线程退出
    g_bExitWrite = true;
    //等待写线程退出
    group.join_all();

    //释放环形缓冲，此时触发Detach事件
    g_ringBuf.reset();
    sleep(1);
    return 0;
}











