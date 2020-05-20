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
#include <atomic>
#include <iostream>
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Thread/ThreadPool.h"

using namespace std;
using namespace toolkit;

int main() {
    signal(SIGINT,[](int ){
        exit(0);
    });
    //初始化日志系统
    Logger::Instance().add(std::make_shared<ConsoleChannel> ());

    atomic_llong count(0);
    ThreadPool pool(1,ThreadPool::PRIORITY_HIGHEST, false);

    Ticker ticker;
    for (int i = 0 ; i < 1000*10000;++i){
        pool.async([&](){
           if(++count >= 1000*10000){
               InfoL << "执行1000万任务总共耗时:" << ticker.elapsedTime() << "ms";
           }
        });
    }
    InfoL << "1000万任务入队耗时:" << ticker.elapsedTime() << "ms" << endl;
    uint64_t  lastCount = 0 ,nowCount = 1;
    ticker.resetTime();
    //此处才开始启动线程
    pool.start();
    while (true){
        sleep(1);
        nowCount = count.load();
        InfoL << "每秒执行任务数:" << nowCount - lastCount;
        if(nowCount - lastCount == 0){
            break;
        }
        lastCount = nowCount;
    }
    return 0;
}
