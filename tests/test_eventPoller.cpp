#include <iostream>
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Poller/EventPoller.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Poller;

int main() {
	//设置日志系统
	Logger::Instance().add(std::make_shared<ConsoleChannel> ("stdout", LTrace));
	Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
	//主线程为自轮询类型，不需要调用runLoop
	EventPoller::Instance(true);

	Ticker timeTicker;//计时器
	TraceL << "main thread id:" << this_thread::get_id();

	DebugL << "start async task"<< endl;
	timeTicker.resetTime();//开始计时

	//异步执行任务
	EventPoller::Instance().async([](){
		//在事件轮询线程执行了休眠一秒的任务
		sleep(1);
		DebugL << "async thread id:" << this_thread::get_id() << endl;
	});

	//这时应该消耗了极短的时间，并没有阻塞当前主线程，timeTicker.elapsedTime()的结果应该接近0毫秒
	DebugL << "async task time:" <<  timeTicker.elapsedTime() << "ms" << endl;

	InfoL << "start sync task"<< endl;
	timeTicker.resetTime();//重新计时

	//同步执行任务，这时会等待任务执行结束
	EventPoller::Instance().sync([](){
		//在事件轮询线程执行了休眠一秒的任务
		sleep(1);
		InfoL << "sync thread id:" << this_thread::get_id()<< endl;
	});

	//此时应该消耗了两秒时间左右，timeTicker.elapsedTime()的结果应该是2000毫秒
	//因为两个任务总共消耗了2秒左右
	InfoL << "sync task time:" <<  timeTicker.elapsedTime() << "ms" << endl;


	//测试结束，清理工作
	EventPoller::Destory();
	Logger::Destory();
	return 0;
}
