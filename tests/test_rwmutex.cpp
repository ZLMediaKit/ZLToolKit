//============================================================================
// Name        : ToolKitTest.cpp
// Author      : xzl
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include "Util/logger.h"
#include "Thread/threadgroup.h"
#include "Thread/rwmutex.h"
#include <atomic>

using namespace std;
using namespace ZL::Util;
using namespace ZL::Thread;

bool g_bExitFlag = false;
rw_mutex g_mutex;
atomic_int g_iWritingCount(0);
atomic_int g_iReadingCount(0);

void programExit(int arg) {
	g_bExitFlag = true;
}

void onRead(int index){
	while(!g_bExitFlag){
		g_mutex.lock(false);
		if( g_iWritingCount.load() !=0 ){
			//如果有在写线程
			FatalL << "failed!";
			abort();
		}
		++g_iReadingCount;
		std::cout << "-";
		usleep(1000);//sleep for 1 ms
		std::cout << "-";
		--g_iReadingCount;
		g_mutex.unlock(false);
		usleep(1000);//sleep for 1 ms
	}
}
void onWrite(int index){
	while(!g_bExitFlag){
		g_mutex.lock(true);
		if(g_iReadingCount.load() != 0 || g_iWritingCount.load() !=0 ){
			//如果有在读和在写线程
			FatalL << "failed!";
			abort();
		}
		++g_iWritingCount;
		std::cout << "#";
		usleep(1000);//sleep for 1 ms
		std::cout << "#";
		--g_iWritingCount;
		g_mutex.unlock(true);
		usleep(1000);//sleep for 1 ms
	}
}
int main() {
	//测试方法：如果程序异常退出说明读写锁异常
	signal(SIGINT, programExit);
	Logger::Instance().add(std::make_shared<ConsoleChannel> ("stdout", LTrace));

	thread_group thread_reader;
	for(int i = 0 ;i < 4 ; ++i){
		thread_reader.create_thread([i](){
			onRead(i);
		});
	}

	thread_group thread_writer;
	for(int i = 0 ;i < 4 ; ++i){
		thread_writer.create_thread([i](){
			onWrite(i);
		});
	}

	thread_reader.join_all();
	thread_writer.join_all();
	Logger::Destory();
	return 0;
}
