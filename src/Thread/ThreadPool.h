/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <assert.h>
#include <vector>
#include "threadgroup.h"
#include "TaskQueue.h"
#include "TaskExecutor.h"
#include "Util/util.h"
#include "Util/logger.h"
namespace toolkit {

class ThreadPool : public TaskExecutor{
public:
    enum Priority {
        PRIORITY_LOWEST = 0,
        PRIORITY_LOW,
        PRIORITY_NORMAL,
        PRIORITY_HIGH,
        PRIORITY_HIGHEST
    };

    //num:线程池线程个数
    ThreadPool(int num = 1,
               Priority priority = PRIORITY_HIGHEST,
               bool autoRun = true) :
            _thread_num(num), _priority(priority) {
        if(autoRun){
            start();
        }
        _logger = Logger::Instance().shared_from_this();
    }
    ~ThreadPool() {
        shutdown();
        wait();
    }

    //把任务打入线程池并异步执行
    Task::Ptr async(TaskIn &&task,bool may_sync = true) override {
        if (may_sync && _thread_group.is_this_thread_in()) {
            task();
            return nullptr;
        }
        auto ret = std::make_shared<Task>(std::move(task));
        _queue.push_task(ret);
        return ret;
    }
    Task::Ptr async_first(TaskIn &&task,bool may_sync = true) override{
        if (may_sync && _thread_group.is_this_thread_in()) {
            task();
            return nullptr;
        }

        auto ret = std::make_shared<Task>(std::move(task));
        _queue.push_task_first(ret);
        return ret;
    }

    uint64_t size(){
        return _queue.size();
    }

    static bool setPriority(Priority priority = PRIORITY_NORMAL,
            thread::native_handle_type threadId = 0) {
        // set priority
#if defined(_WIN32)
        static int Priorities[] = { THREAD_PRIORITY_LOWEST, THREAD_PRIORITY_BELOW_NORMAL, THREAD_PRIORITY_NORMAL, THREAD_PRIORITY_ABOVE_NORMAL, THREAD_PRIORITY_HIGHEST };
        if (priority != PRIORITY_NORMAL && SetThreadPriority(GetCurrentThread(), Priorities[priority]) == 0) {
            return false;
        }
        return true;
#else
        static int Min = sched_get_priority_min(SCHED_OTHER);
        if (Min == -1) {
            return false;
        }
        static int Max = sched_get_priority_max(SCHED_OTHER);
        if (Max == -1) {
            return false;
        }
        static int Priorities[] = { Min, Min + (Max - Min) / 4, Min
            + (Max - Min) / 2, Min + (Max - Min) * 3/ 4, Max };

        if (threadId == 0) {
            threadId = pthread_self();
        }
        struct sched_param params;
        params.sched_priority = Priorities[priority];
        return pthread_setschedparam(threadId, SCHED_OTHER, &params) == 0;
#endif
    }

    void start() {
        if (_thread_num <= 0)
            return;
        auto total =  _thread_num - _thread_group.size();
        for (int i = 0; i < total; ++i) {
            _thread_group.create_thread(bind(&ThreadPool::run, this));
        }
    }

private:
    void run() {
        ThreadPool::setPriority(_priority);
        Task::Ptr task;
        while (true) {
            startSleep();
            if (!_queue.get_task(task)) {
                //空任务，退出线程
                break;
            }
            sleepWakeUp();
            try {
                (*task)();
                task = nullptr;
            } catch (std::exception &ex) {
                ErrorL << "ThreadPool执行任务捕获到异常:" << ex.what();
            }
        }
    }

    void wait() {
        _thread_group.join_all();
    }

    void shutdown() {
        _queue.push_exit(_thread_num);
    }
private:
    TaskQueue<Task::Ptr> _queue;
    thread_group _thread_group;
    int _thread_num;
    Priority _priority;
    Logger::Ptr _logger;
};

} /* namespace toolkit */
#endif /* THREADPOOL_H_ */
