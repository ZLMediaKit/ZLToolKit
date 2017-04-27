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
#include "Thread/threadgroup.h"
#include "Thread/semaphore.h"
#include <list>

using namespace std;
using namespace ZL::Util;
using namespace ZL::Thread;

bool g_bExitFlag = false;
semaphore g_sem;
mutex g_mtxList;
list<int> g_list;

void programExit(int arg) {
	g_bExitFlag = true;
	for(int i = 0 ; i < 10 ;++i){
		//let consumer threads exit!
		lock_guard<mutex> lck(g_mtxList);
		g_list.push_front(i++);
		g_sem.post();
	}
}

void onConsum(int index) {
	while (!g_bExitFlag) {
		g_sem.wait();
		lock_guard<mutex> lck(g_mtxList);
		DebugL << "thread " << index << ":" << g_list.back() << endl;
		g_list.pop_back();
	}
}
void onProduce() {
	int i = 0;
	while (!g_bExitFlag) {
		{
			lock_guard<mutex> lck(g_mtxList);
			g_list.push_front(i++);
		}
		g_sem.post();
		usleep(1000); // sleep 1 ms
	}
}
int main() {
	signal(SIGINT, programExit);
	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));

	thread_group thread_producer;
	thread_producer.create_thread([]() {
		//生产者
		onProduce();
	});

	thread_group thread_consumer;
	for (int i = 0; i < 4; ++i) {
		thread_consumer.create_thread([i]() {
			//消费者
			onConsum(i);
		});
	}
	thread_consumer.join_all();
	thread_producer.join_all();
	Logger::Destory();
	return 0;
}
