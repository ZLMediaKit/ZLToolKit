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
#include "Util/onceToken.h"
#include "Util/TimeTicker.h"
#include "Thread/ThreadPool.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Thread;

#ifndef UINT32_MAX
#define UINT32_MAX 0xFFFFFFFF
#endif

int main() {
	signal(SIGINT, [](int){});
	//初始化log
	Logger::Instance().add(std::make_shared<ConsoleChannel> ("stdout", LTrace));
	Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

	Ticker timeTicker;//计时器
	TraceL << "main thread id:" << this_thread::get_id();

	DebugL << "start async task"<< endl;
	timeTicker.resetTime();//计时器重置计时

	//后台线程异步执行一个任务
	ThreadPool::Instance().async([](){
		sleep(1);//该任务是休眠一秒并打印log
		DebugL << "async thread id:" << this_thread::get_id() << endl;
	});

	//后台线程任务不阻塞当前主线程，此时timeTicker.elapsedTime()结果应该接近0毫秒
	DebugL << "async task take time:" <<  timeTicker.elapsedTime() << "ms" << endl;

	InfoL << "start sync task"<< endl;
	timeTicker.resetTime();//计时器重置计时

	//同步执行任务，将等待任务执行完毕
	ThreadPool::Instance().sync([](){
		sleep(1);
		InfoL << "sync thread id:" << this_thread::get_id()<< endl;
	});

	//两次任务执行耗时2秒，timeTicker.elapsedTime()结果接近2000毫秒
	InfoL << "sync task take time:" <<  timeTicker.elapsedTime() << "ms" << endl;

	sleep(UINT32_MAX);//sleep会被Ctl+C打断，可以正常退出程序

	//等待线程池退出
	ThreadPool::Instance().wait();
	DebugL << "exited!" << endl;
	Logger::Destory();
	return 0;
}
