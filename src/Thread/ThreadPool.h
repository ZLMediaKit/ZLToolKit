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
	}
	~ThreadPool() {
		shutdown();
		wait();
	}

	//把任务打入线程池并异步执行
	bool async(const Task &task,bool may_sync = true) override {
		if (may_sync && _thread_group.is_this_thread_in()) {
			task();
		} else {
			_queue.push_task(task);
		}
		return true;
	}
	bool async_first(const Task &task,bool may_sync = true) override{
		if (may_sync && _thread_group.is_this_thread_in()) {
			task();
		} else {
			_queue.push_task_first(task);
		}
		return true;
	}
	bool sync(const Task &task) override{
		semaphore sem;
		bool flag = async([&](){
			task();
			sem.post();
		});
		if(flag){
			sem.wait();
		}
		return flag;
	}
	bool sync_first(const Task &task) override{
		semaphore sem;
		bool flag = async_first([&]() {
			task();
			sem.post();
		});
		if (flag) {
			sem.wait();
		}
		return flag;
	}

	void wait() override{
		_thread_group.join_all();
	}

    uint64_t size(){
        return _queue.size();
    }

    void shutdown() override{
		_queue.push_exit(_thread_num);
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
		TaskExecutor::Task task;
		while (true) {
			startSleep();
			if (!_queue.get_task(task)) {
                //空任务，退出线程
                break;
            }
            sleepWakeUp();
            try {
                task();
                task = nullptr;
            } catch (std::exception &ex) {
				ErrorL << "catch exception:" << ex.what();
            }
		}
	}
private:
	TaskQueue<TaskExecutor::Task> _queue;
	thread_group _thread_group;
	int _thread_num;
	Priority _priority;
};

} /* namespace toolkit */

#endif /* THREADPOOL_H_ */
