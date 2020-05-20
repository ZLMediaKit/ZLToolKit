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
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Poller/EventPoller.h"

using namespace std;
using namespace toolkit;

/**
 * cpu负载均衡测试
 * @return
 */
int main() {
    static bool  exit_flag = false;
    signal(SIGINT, [](int) { exit_flag = true; });
    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>());

    Ticker ticker;
    while(!exit_flag){

        if(ticker.elapsedTime() > 1000){
            auto vec = EventPollerPool::Instance().getExecutorLoad();
            _StrPrinter printer;
            for(auto load : vec){
                printer << load << "-";
            }
            DebugL << "cpu负载:" << printer;

            EventPollerPool::Instance().getExecutorDelay([](const vector<int> &vec){
                _StrPrinter printer;
                for(auto delay : vec){
                    printer << delay << "-";
                }
                DebugL << "cpu任务执行延时:" << printer;
            });
            ticker.resetTime();
        }

        EventPollerPool::Instance().getExecutor()->async([](){
            auto usec = rand() % 4000;
            //DebugL << usec;
            usleep(usec);
        });

        usleep(2000);
    }

    return 0;
}
