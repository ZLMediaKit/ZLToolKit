#include <signal.h>
#include <stdio.h>
#include <iostream>
#include "Util/logger.h"
#include "Thread/threadgroup.h"
#include "Thread/rwmutex.h"
#include <atomic>

using namespace std;
using namespace ZL::Util;
using namespace ZL::Thread;

bool g_bExitFlag = false;//程序退出标志
rw_mutex g_mutex;//读写锁
atomic_int g_iWritingCount(0);//正在写线程个数
atomic_int g_iReadingCount(0);//正在读线程个数


//读线程执行函数
void onRead(int index){
	while(!g_bExitFlag){
		g_mutex.lock(false);//获取读取锁
		if( g_iWritingCount.load() !=0 ){
			//如果有线程正在写入，说明测试失败，读写锁没有达到设计预期
			FatalL << "test failed!" << endl;
			abort();
		}
		++g_iReadingCount; //读取线程数量加1
		std::cout << "-";
		usleep(1000);//sleep for 1 ms
		std::cout << "-";
		--g_iReadingCount; ///读取线程数量减1
		g_mutex.unlock(false);//释放读取锁
		usleep(1000);//sleep for 1 ms
	}
}

//写线程执行函数
void onWrite(int index){
	while(!g_bExitFlag){
		g_mutex.lock(true);//获取写入锁
		if(g_iReadingCount.load() != 0 || g_iWritingCount.load() !=0 ){
			//获取写入锁时这时不应该有其他线程在读写，否则测试失败
			FatalL << "test failed!" << endl;
			abort();
		}
		++g_iWritingCount; //写入线程数加1
		std::cout << "+";
		usleep(1000);//sleep for 1 ms
		std::cout << "+";
		--g_iWritingCount;//写入线程数减1
		g_mutex.unlock(true);
		usleep(1000);//sleep for 1 ms
	}
}
int main() {
	//测试方法:如果程序异常退出说明读写锁异常
	signal(SIGINT, [](int){g_bExitFlag = true;});
	Logger::Instance().add(std::make_shared<ConsoleChannel> ("stdout", LTrace));

	thread_group thread_reader;
	//创建4个线程同时读取
	for(int i = 0 ;i < 4 ; ++i){
		thread_reader.create_thread([i](){
			onRead(i);
		});
	}

	thread_group thread_writer;
	//创建4个线程同时写入
	for(int i = 0 ;i < 4 ; ++i){
		thread_writer.create_thread([i](){
			onWrite(i);
		});
	}

	//等待所有线程退出
	thread_reader.join_all();
	thread_writer.join_all();
	Logger::Destory();
	return 0;
}
