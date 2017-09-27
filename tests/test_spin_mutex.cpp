#include <signal.h>
#include <iostream>
#include "Util/logger.h"
#include "Thread/threadgroup.h"
#include "Thread/semaphore.h"
#include "Thread/spin_mutex.h"
#include <list>

using namespace std;
using namespace ZL::Util;
using namespace ZL::Thread;

bool g_bExitFlag = false;//程序退出标志
semaphore g_sem;//信号量
spin_mutex g_mtxList;//互斥锁
list<int> g_list;//列队

void programExit(int arg) {
    g_bExitFlag = true;//程序退出标志置位
    for(int i = 0 ; i < 10 ;++i){
        //发送10次信号，唤醒所有线程并退出
        lock_guard<spin_mutex> lck(g_mtxList);
        g_list.push_front(i++);
        g_sem.post();
    }
}

//消费者线程
void onConsum(int index) {
    while (!g_bExitFlag) {
        g_sem.wait();//等待生产者生产
        lock_guard<spin_mutex> lck(g_mtxList);
        DebugL << "thread " << index << ":" << g_list.back() << endl;
        g_list.pop_back();//消费一个
    }
}

//生产者线程
void onProduce() {
    int i = 0;
    while (!g_bExitFlag) {
        {
            lock_guard<spin_mutex> lck(g_mtxList);
            g_list.push_front(i++);//生产一个
        }
        g_sem.post();//通知消费者线程
        usleep(1000); // sleep 1 ms
    }
}
int main() {
    //设置程序退出信号处理函数
    signal(SIGINT, programExit);
    //初始化log
    Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));

    thread_group thread_producer;
    thread_producer.create_thread([]() {
        //1个生产者线程
        onProduce();
    });

    thread_group thread_consumer;
    for (int i = 0; i < 4; ++i) {
        thread_consumer.create_thread([i]() {
            //4个消费者线程
            onConsum(i);
        });
    }

    //等待所有线程退出
    thread_consumer.join_all();
    thread_producer.join_all();

    //清理程序
    Logger::Destory();
    return 0;
}
