/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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
#include <atomic>
#include <Util/TimeTicker.h>
#include "Util/logger.h"
#include "Thread/threadgroup.h"
#include "Thread/semaphore.h"

using namespace std;
using namespace toolkit;


#define MAX_TASK_SIZE (1000 * 10000)
semaphore g_sem;//信号量
atomic_llong g_produced(0);
atomic_llong g_consumed(0);

//消费者线程
void onConsum() {
	while (true) {
		g_sem.wait();
		if (++g_consumed > g_produced) {
			//如果打印这句log则表明有bug
			ErrorL << g_consumed << " > " << g_produced;
		}
	}
}

//生产者线程
void onProduce() {
	while(true){
		++ g_produced;
		g_sem.post();
		if(g_produced >= MAX_TASK_SIZE){
		    break;
		}
	}
}
int main() {
	//初始化log
	Logger::Instance().add(std::make_shared<ConsoleChannel>());

	Ticker ticker;
	thread_group thread_producer;
    for (int i = 0; i < thread::hardware_concurrency(); ++i) {
        thread_producer.create_thread([]() {
            //1个生产者线程
            onProduce();
        });
    }

	thread_group thread_consumer;
	for (int i = 0; i < 4; ++i) {
		thread_consumer.create_thread([i]() {
			//4个消费者线程
			onConsum();
		});
	}



	//等待所有生成者线程退出
	thread_producer.join_all();
	DebugL << "生产者线程退出，耗时:" << ticker.elapsedTime() << "ms," << "生产任务数:" << g_produced << ",消费任务数:" << g_consumed;

	int i = 5;
	while(-- i){
		DebugL << "程序退出倒计时:" << i << ",消费任务数:" << g_consumed;
		sleep(1);
	}

	//程序强制退出可能core dump；在程序推出时，生产的任务数应该跟消费任务数一致
	WarnL << "强制关闭消费线程，可能触发core dump" ;
	return 0;
}
