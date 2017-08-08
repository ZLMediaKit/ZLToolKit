//============================================================================
// Name        : ToolKitTest.cpp
// Author      : xzl
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Poller/EventPoller.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Poller;

int main() {
	Logger::Instance().add(std::make_shared<ConsoleChannel> ("stdout", LTrace));
	Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
	EventPoller::Instance(true);
	Ticker timeTicker;
	TraceL << "main thread id:" << this_thread::get_id();

	DebugL << "start async task"<< endl;
	timeTicker.resetTime();
	EventPoller::Instance().async([](){
		sleep(1);
		DebugL << "async thread id:" << this_thread::get_id() << endl;
	});
	DebugL << "async task time:" <<  timeTicker.elapsedTime() << "ms" << endl;

	InfoL << "start sync task"<< endl;
	timeTicker.resetTime();
	EventPoller::Instance().sync([](){
		sleep(1);
		InfoL << "sync thread id:" << this_thread::get_id()<< endl;
	});
	InfoL << "sync task time:" <<  timeTicker.elapsedTime() << "ms" << endl;


	EventPoller::Destory();
	Logger::Destory();
	return 0;
}
