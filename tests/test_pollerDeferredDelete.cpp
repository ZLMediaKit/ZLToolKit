/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

//回归:EventPoller的最后一个引用在它自己的轮询线程上、且是在延时任务里释放时,销毁必须被推迟到
//runLoop返回之后(靠写一字节管道唤醒循环),并且仍发生在该轮询线程自己身上。三个判据缺一不可:
//  1. 两秒内销毁了(推迟了但没唤醒时,循环会卡在epoll_wait里,对象永久残留);
//  2. 销毁发生在轮询线程上(发生在主线程意味着根本没走到推迟路径,用例无效);
//  3. 释放引用的当场对象还活着(当场就销毁说明没有推迟,那正是修改前会读到已释放内存的行为)。
//Regression: when the last reference to an EventPoller goes away on its own polling thread from
//inside a delayed task, destruction has to be deferred until runLoop has returned (the loop is woken
//through the pipe) and still happen on that polling thread. All three checks are needed:
//  1. destroyed within two seconds (deferred but not woken, the loop stays in epoll_wait for good);
//  2. destroyed on the polling thread (on the main thread the deferred path was never taken);
//  3. still alive right after the reference was dropped (destroyed on the spot means nothing was
//     deferred, which is the pre-fix behaviour that read freed memory).
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

#include "Poller/EventPoller.h"
#include "Util/logger.h"

using namespace std;
using namespace toolkit;

static mutex g_mtx;
static thread::id g_dtor_tid;
static atomic<bool> g_dtor_seen { false };
static atomic<bool> g_dtor_seen_at_release { false };
static atomic<bool> g_release_done { false };

//捕获~EventPoller打出的那条日志,记下它发生在哪个线程;用例没有设置异步写日志,
//所以write()与析构在同一线程同步执行
//Capture the log line printed by ~EventPoller and note the thread it ran on; no async writer is
//set, so write() runs synchronously on the destroying thread
class CaptureChannel : public LogChannel {
public:
    CaptureChannel() : LogChannel("capture", LTrace) {}
    void write(const Logger &, const LogContextPtr &ctx) override {
        if (ctx->_function.find("~EventPoller") != string::npos) {
            lock_guard<mutex> lck(g_mtx);
            g_dtor_tid = this_thread::get_id();
            g_dtor_seen = true;
        }
    }
};

//成员按声明的逆序析构:poller先释放(触发删除器),随后guard才析构。若那时析构日志已经出现,
//说明释放引用的当场就把对象销毁了,即没有推迟。主线程要等g_release_done也置位后才能读结论,
//否则在"当场销毁"这条坏路径上,若轮询线程刚打完日志就被抢占,主线程会读到尚未写入的默认值
//Members are destroyed in reverse order of declaration: poller goes first (running the deleter),
//guard afterwards. If the destructor log line is already there by then, the object was destroyed
//on the spot, i.e. nothing was deferred. The main thread must also wait for g_release_done before
//reading the verdict, otherwise on the bad "destroyed on the spot" path a polling thread preempted
//right after logging would leave the main thread reading the not-yet-written default
struct Guard {
    ~Guard() {
        g_dtor_seen_at_release = g_dtor_seen.load();
        g_release_done = true;
    }
};
struct Holder {
    Guard guard;
    EventPoller::Ptr poller;
};

struct Pool : public TaskExecutorGetterImp {
    Pool() { addPoller("deferred", 1, ThreadPool::PRIORITY_NORMAL, false, false); }
    EventPoller::Ptr poller() { return static_pointer_cast<EventPoller>(_threads[0]); }
};

int main() {
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().add(std::make_shared<CaptureChannel>());

    auto pool = new Pool();
    auto holder = make_shared<Holder>();
    holder->poller = pool->poller();
    auto poller_tid = holder->poller->getThreadId();
    //延时任务的闭包持有holder;任务执行完被销毁时,holder里的那份就是poller的最后一个引用,
    //且释放发生在轮询线程上
    //The closure of the delayed task holds the holder; when the task is destroyed after running,
    //the copy inside is the last reference to the poller, dropped on the polling thread
    holder->poller->doDelayTask(50, [holder]() { return 0; });
    holder.reset();
    //立刻释放池子,此时延时任务尚未到期
    //Release the pool right away, the delayed task is not due yet
    delete pool;

    for (int i = 0; i < 100 && !(g_dtor_seen && g_release_done); ++i) {
        this_thread::sleep_for(chrono::milliseconds(20));
    }
    if (!g_dtor_seen) {
        cerr << "poller not destroyed within 2s" << endl;
        return 1;
    }
    lock_guard<mutex> lck(g_mtx);
    if (g_dtor_tid != poller_tid) {
        cerr << "poller destroyed on a thread other than its own polling thread" << endl;
        return 2;
    }
    if (g_dtor_seen_at_release) {
        cerr << "poller destroyed on the spot when the last reference was dropped, nothing was deferred" << endl;
        return 3;
    }
    cout << "deferred delete regression passed" << endl;
    return 0;
}
