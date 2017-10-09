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
#include <iostream>
#include "Util/logger.h"
#include "Poller/EventPoller.h"
#include "Poller/Pipe.h"
#include "Util/util.h"
#include "Thread/AsyncTaskThread.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Poller;


int main() {
	//设置程序退出信号处理函数
	signal(SIGINT, [](int){EventPoller::Instance().shutdown();});
	//设置日志系统
	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
	Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

	std::shared_ptr<int> pCount1(new int(0));
	//设置一个定时任务，任务标记为1
	AsyncTaskThread::Instance().DoTaskDelay(1,1000,[pCount1](){
		//该任务是打印一段日志，每隔1秒执行一次
		DebugL << "timer type 1:" << ++(*pCount1);
		return true;//返回true代表下次(1秒后)再次持续该任务，否则停止重复
	});

	//设置一个定时任务，任务标记也是为1(可以重名)，跟上面的任务不冲突
	AsyncTaskThread::Instance().DoTaskDelay(1,1000,[](){
		//该任务是打印一段日志，每隔1秒执行一次
		DebugL << "timer type 1";
		return true;//重复任务
	});

	//创建一个任务5秒后执行
	AsyncTaskThread::Instance().DoTaskDelay(2,5000,[](){
		//取消所有标记为1的任务
		AsyncTaskThread::Instance().CancelTask(1);
		DebugL << "all timer was canceled after 5 second";
		return false;//该任务只执行一次
	});

	EventPoller::Instance().runLoop();//主线程事件轮询

	//程序开始退出，做些清理工作
	EventPoller::Destory();
	Logger::Destory();
	return 0;
}







