/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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
