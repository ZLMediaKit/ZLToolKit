/*
 * ThreadPool.h
 *
 *  Created on: 2013-10-11
 *      Author: root
 */

#ifndef THREADPOOL_H_
#define THREADPOOL_H_
//************************************************************************
//Remarks:
//线程池使用boost中的thread_group来管理和创建工作线程，使其阻塞在任队列中
//
//*************************************************************************

#include <assert.h>
#include <Thread/threadgroup.hpp>
#include <Thread/TaskQueue.hpp>
#include <Thread/TaskQueue.hpp>
#include <vector>

namespace ZL {
namespace Thread {
class ThreadPool {
public:
	enum Priority {
		PRIORITY_LOWEST = 0,
		PRIORITY_LOW,
		PRIORITY_NORMAL,
		PRIORITY_HIGH,
		PRIORITY_HIGHEST
	};

	//num：线程池线程个数
	ThreadPool(int num, Priority _priority = PRIORITY_NORMAL) :
			thread_num(num), avaible(true), priority(_priority) {
		start();
	}
	~ThreadPool() {
		wait();
	}

	//把任务打入线程池并异步执行
	void post(const Task & task) {
		if (!avaible.load() || !task) {
			return;
		}
		if (my_thread_group.is_this_thread_in()) {
			task();
		} else {
			my_queue.push_task(task);
		}
	}
	void post_first(const Task & task) {
		if (!avaible.load() || !task) {
			return;
		}
		if (my_thread_group.is_this_thread_in()) {
			task();
		} else {
			my_queue.push_task_first(task);
		}
	}
	//同步等待线程池执行完所有任务并退出
	void wait() {
		exit();
		my_thread_group.join_all();
	}
	static ThreadPool &Instance() {
		//单例模式
		static ThreadPool instance(thread::hardware_concurrency());
		return instance;
	}
	static bool setPriority(Priority _priority = PRIORITY_NORMAL,
			thread::native_handle_type threadId = 0) {
		static int Min = sched_get_priority_min(SCHED_OTHER);
		if (Min == -1) {
			return false;
		}
		static int Max = sched_get_priority_max(SCHED_OTHER);
		if (Max == -1) {
			return false;
		}
		static int Priorities[] = { Min, Min + (Max - Min) / 4, Min
				+ (Max - Min) / 2, Min + (Max - Min) / 4, Max };

		if (threadId == 0) {
			threadId = pthread_self();
		}
		struct sched_param params;
		params.sched_priority = Priorities[_priority];
		return pthread_setschedparam(threadId, SCHED_OTHER, &params) == 0;
	}
private:
	TaskQueue my_queue;
	thread_group my_thread_group;
	int thread_num;
	atomic_bool avaible;
	Priority priority;
	//发送空任务至任务列队，通知线程主动退出
	void exit() {
		avaible = false;
		my_queue.push_exit(thread_num);
	}
	void start() {
		if (thread_num <= 0)
			return;
		for (int i = 0; i < thread_num; ++i) {
			my_thread_group.create_thread(bind(&ThreadPool::run, this));
		}
	}
	void run() {
		ThreadPool::setPriority(priority);
		Task task;
		while (true) {
			if (my_queue.get_task(task)) {
				task();
				task=nullptr;
			} else {
				//空任务，退出线程
				break;
			}
		}
	}
}
;

} /* namespace Thread */
} /* namespace ZL */
#endif /* THREADPOOL_H_ */
