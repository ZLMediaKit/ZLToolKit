/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <signal.h>
#include <atomic>
#include <iostream>
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Thread/ThreadPool.h"

using namespace std;
using namespace ZL::Util;


int main() {
    signal(SIGINT,[](int ){
        Logger::Destory();
        exit(0);
    });
    //初始化日志系统
    Logger::Instance().add(std::make_shared<ConsoleChannel> ("stdout", LTrace));

    atomic_llong count(0);
    ThreadPool pool(1,ThreadPool::PRIORITY_HIGHEST, false);

    Ticker ticker;
    for (int i = 0 ; i < 1000*10000;++i){
        pool.async([&](){
           if(++count >= 1000*10000){
               InfoL << "总共耗时:" << ticker.elapsedTime() << "," << count;
           }
        });
    }
    InfoL << "ThreadPool::async耗时:" << ticker.elapsedTime() << endl;
    uint64_t  lastCount = 0 ,nowCount = 0;
    ticker.resetTime();
    //此处才开始启动线程
    pool.start();
    while (true){
        sleep(1);
        nowCount = count.load();
        InfoL << nowCount - lastCount;
        lastCount = nowCount;
    }
    return 0;
}
