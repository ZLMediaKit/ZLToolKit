/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <csignal>
#include <atomic>
#include "Util/TimeTicker.h"
#include "Util/logger.h"
#include "Thread/threadgroup.h"
#include "Thread/semaphore.h"

using namespace std;
using namespace toolkit;


#define MAX_TASK_SIZE (1000 * 10000)
semaphore g_sem;//信号量
atomic_llong g_produced(0);
atomic_llong g_consumed(0);

//消费者线程  [AUTO-TRANSLATED:db1fb231]
// Consumer thread
void onConsum() {
    while (true) {
        g_sem.wait();
        if (++g_consumed > g_produced) {
            //如果打印这句log则表明有bug  [AUTO-TRANSLATED:190165e0]
            // If this log is printed, it indicates a bug
            ErrorL << g_consumed << " > " << g_produced;
        }
    }
}

//生产者线程  [AUTO-TRANSLATED:3f37e15e]
// Producer thread
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
    //初始化log  [AUTO-TRANSLATED:585df223]
    // Initialize log
    Logger::Instance().add(std::make_shared<ConsoleChannel>());

    Ticker ticker;
    thread_group thread_producer;
    for (size_t i = 0; i < thread::hardware_concurrency(); ++i) {
        thread_producer.create_thread([]() {
            //1个生产者线程  [AUTO-TRANSLATED:a9a01b88]
            // 1 producer thread
            onProduce();
        });
    }

    thread_group thread_consumer;
    for (int i = 0; i < 4; ++i) {
        thread_consumer.create_thread([i]() {
            //4个消费者线程  [AUTO-TRANSLATED:9dabe877]
            // 4 consumer threads
            onConsum();
        });
    }



    //等待所有生成者线程退出  [AUTO-TRANSLATED:321d2677]
    // Wait for all producer threads to exit
    thread_producer.join_all();
    DebugL << "生产者线程退出，耗时:" << ticker.elapsedTime() << "ms," << "生产任务数:" << g_produced << ",消费任务数:" << g_consumed;

    int i = 5;
    while(-- i){
        DebugL << "程序退出倒计时:" << i << ",消费任务数:" << g_consumed;
        sleep(1);
    }

    //程序强制退出可能core dump；在程序推出时，生产的任务数应该跟消费任务数一致  [AUTO-TRANSLATED:59593bf0]
    // The program may force exit and core dump; when the program exits, the number of produced tasks should be consistent with the number of consumed tasks
    WarnL << "强制关闭消费线程，可能触发core dump" ;
    return 0;
}
