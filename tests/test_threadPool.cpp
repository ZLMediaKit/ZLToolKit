#include <signal.h>
#include <iostream>
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Util/TimeTicker.h"
#include "Thread/ThreadPool.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Thread;


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
