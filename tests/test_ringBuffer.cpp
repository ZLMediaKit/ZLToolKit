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
#include "Util/util.h"
#include "Util/RingBuffer.h"
#include "Thread/threadgroup.h"
#include <list>

using namespace std;
using namespace ZL::Util;
using namespace ZL::Thread;


bool g_bExitRead = false;
bool g_bExitWrite = false;
RingBuffer<string>::Ptr g_ringBuf(new RingBuffer<string>());


void onReadEvent(const string &str){
	//读事件模式性
	DebugL << str;
}
void onDetachEvent(){
	WarnL;
}

void doWrite(){
	int i = 0;
	while(!g_bExitWrite){
		g_ringBuf->write(to_string(++i),true);
		usleep(100 * 1000);
	}

}
int main() {
	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
	//Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

	//添加一个读取器
	auto ringReader = g_ringBuf->attach();
	ringReader->setReadCB([](const string &pkt){
		onReadEvent(pkt);
	});
	ringReader->setDetachCB([](){
		onDetachEvent();
	});

	thread_group group;
	//写线程
	group.create_thread([](){
		doWrite();
	});

	sleep(1);
	//写线程退出
	g_bExitWrite = true;
	sleep(1);
	//释放环形缓冲
	g_ringBuf.reset();

	sleep(1);
	g_bExitRead = true;
	group.join_all();

	Logger::Destory();
	return 0;
}











