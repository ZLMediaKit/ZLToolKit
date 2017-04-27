//============================================================================
// Name        : ToolKitTest.cpp
// Author      : xzl
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include "Util/logger.h"
#include "Poller/EventPoller.h"
#include "Util/TimeTicker.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Poller;

int main() {
	Logger::Instance().add(std::make_shared<ConsoleChannel> ("stdout", LTrace));
	Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
	EventPoller::Instance(true);
	Ticker timeTicker;
	TraceL << "主线程id:" << this_thread::get_id();

	DebugL << "开始异步操作"<< endl;
	timeTicker.resetTime();
	EventPoller::Instance().async([](){
		sleep(1);
		DebugL << "异步操作:" << this_thread::get_id() << endl;
	});
	DebugL << "异步操作消耗时间：" <<  timeTicker.elapsedTime() << "ms" << endl;

	InfoL << "开始同步操作"<< endl;
	timeTicker.resetTime();
	EventPoller::Instance().sync([](){
		sleep(1);
		InfoL << "同步操作:" << this_thread::get_id()<< endl;
	});
	InfoL << "同步操作消耗时间：" <<  timeTicker.elapsedTime() << "ms" << endl;


	EventPoller::Destory();
	Logger::Destory();
	return 0;
}
