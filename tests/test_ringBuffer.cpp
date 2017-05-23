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

#ifdef ANDROID
#define to_string(arg) (StrPrinter << (arg) << endl)
#endif//ANDROID

bool g_bExitRead = false;
bool g_bExitWrite = false;
RingBuffer<string>::Ptr g_ringBuf(new RingBuffer<string>(48));


void onReadEvent(const string &str){
	//读事件模式性
	DebugL << str;
}
void onDetachEvent(){
	WarnL;
}
void doRead(int threadNum){
	//主动读模式采用轮训机制 效率比较差，可以加入条件变量机制改造
	auto reader = g_ringBuf->attach();
	while(!g_bExitRead){
		auto ptr = reader->read();
		if(ptr){
			InfoL << "thread " << threadNum << ":" << *ptr;
		}else{
			InfoL << "thread " << threadNum << ": read nullptr!";
			usleep(100 * 1000);
		}
	}
}
void doWrite(){
	int i = 0;
	while(!g_bExitWrite){
		g_ringBuf->write(to_string(++i));
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

	//主动读取线程
	thread_group group;
	for(int i = 0 ;i < 4 ; ++i){
		group.create_thread([i](){
			doRead(i);
		});
	}

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











