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
#include <iostream>
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Poller/EventPoller.h"
#include "Thread/AsyncTaskThread.h"

using namespace std;
using namespace toolkit;

/**
 * cpu负载均衡测试
 * @return
 */
int main() {
	//设置日志系统
	Logger::Instance().add(std::make_shared<ConsoleChannel> ("stdout", LTrace));
	Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

	AsyncTaskThread::Instance(2).DoTaskDelay(0,2,[](){
		EventPollerPool::Instance().getExecutor()->async([](){
		    auto usec = rand() % 5000;
		    //DebugL << usec;
			usleep(usec);
		});
		return true;
	});

	while(true){
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
		sleep(1);
	}

	//测试结束，清理工作
	EventPoller::Destory();
	Logger::Destory();
	return 0;
}
