//============================================================================
// Name        : ToolKitTest.cpp
// Author      : xzl
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <signal.h>
#include <iostream>
#include "Util/logger.h"
#include "Thread/ThreadPool.h"
#include "Util/TimeTicker.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Thread;

bool g_bExitFlag = false;
void programExit(int arg) {
	g_bExitFlag = true;
}
int main() {
	signal(SIGINT, programExit);
	Logger::Instance().add(std::make_shared<ConsoleChannel> ("stdout", LTrace));
	Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

	Ticker timeTicker;
	TraceL << "main thread id:" << this_thread::get_id();

	DebugL << "start async task"<< endl;
	timeTicker.resetTime();
	ThreadPool::Instance().async([](){
		sleep(1);
		DebugL << "async thread id:" << this_thread::get_id() << endl;
	});
	DebugL << "async task take time:" <<  timeTicker.elapsedTime() << "ms" << endl;

	InfoL << "start sync task"<< endl;
	timeTicker.resetTime();
	ThreadPool::Instance().sync([](){
		sleep(1);
		InfoL << "sync thread id:" << this_thread::get_id()<< endl;
	});
	InfoL << "sync task take time:" <<  timeTicker.elapsedTime() << "ms" << endl;

	while(!g_bExitFlag){
		sleep(1);
	}
	ThreadPool::Instance().wait();
	Logger::Destory();
	return 0;
}
