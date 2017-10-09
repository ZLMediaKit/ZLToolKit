/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <signal.h>
#include <iostream>
#include "Util/logger.h"
#include "Util/util.h"
#include "Util/RingBuffer.h"
#include "Thread/threadgroup.h"
#include <list>

using namespace std;
using namespace ZL::Util;
using namespace ZL::Thread;

//环形缓存写线程退出标记
bool g_bExitWrite = false;

//一个30个string对象的环形缓存
RingBuffer<string>::Ptr g_ringBuf(new RingBuffer<string>(30));

//写事件回调函数
void onReadEvent(const string &str){
	//读事件模式性
	DebugL << str;
}

//环形缓存销毁事件
void onDetachEvent(){
	WarnL;
}

//写环形缓存任务
void doWrite(){
	int i = 0;
	while(!g_bExitWrite){
		//每隔100ms写一个数据到环形缓存
		g_ringBuf->write(to_string(++i),true);
		usleep(100 * 1000);
	}

}
int main() {
	//初始化日志
	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
	Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

	//从环形缓存获取一个读取器
	auto ringReader = g_ringBuf->attach();

	//设置读取事件
	ringReader->setReadCB([](const string &pkt){
		onReadEvent(pkt);
	});

	//设置环形缓存销毁事件
	ringReader->setDetachCB([](){
		onDetachEvent();
	});

	thread_group group;
	//写线程
	group.create_thread([](){
		doWrite();
	});

	//测试3秒钟
	sleep(3);

	//通知写线程退出
	g_bExitWrite = true;
	//等待写线程退出
	group.join_all();

	//释放环形缓冲，此时触发Detach事件
	g_ringBuf.reset();

	Logger::Destory();
	return 0;
}











