//============================================================================
// Name        : ToolKitTest.cpp
// Author      : xzl
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <signal.h>
#include <unistd.h>
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
	TraceL << "主线程id:" << this_thread::get_id();

	DebugL << "开始异步操作"<< endl;
	timeTicker.resetTime();
	ThreadPool::Instance().async([](){
		sleep(1);
		DebugL << "异步操作:" << this_thread::get_id() << endl;
	});
	DebugL << "异步操作消耗时间：" <<  timeTicker.elapsedTime() << "ms" << endl;

	InfoL << "开始同步操作"<< endl;
	timeTicker.resetTime();
	ThreadPool::Instance().sync([](){
		sleep(1);
		InfoL << "同步操作:" << this_thread::get_id()<< endl;
	});
	InfoL << "同步操作消耗时间：" <<  timeTicker.elapsedTime() << "ms" << endl;

	while(!g_bExitFlag){
		sleep(1);
	}
	ThreadPool::Instance().wait();
	Logger::Destory();
	return 0;
}
