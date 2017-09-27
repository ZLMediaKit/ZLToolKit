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

#ifndef TASKQUEUE_H_
#define TASKQUEUE_H_

#include <deque>
#include <atomic>
#include <mutex>
#include <functional>
#include "spin_mutex.h"
#include "semaphore.h"

using namespace std;

namespace ZL {
namespace Thread {

//实现了一个基于函数对象的任务列队，该列队是线程安全的，任务列队任务数由信号量控制
class TaskQueue {
public:
	TaskQueue() {
	}
	//打入任务至列队
	template <typename T>
	void push_task(T &&task_func) {
		{
			lock_guard<spin_mutex> lock(my_mutex);
			my_queue.emplace_back(std::forward<T>(task_func));
		}
		sem.post();
	}
	template <typename T>
	void push_task_first(T &&task_func) {
		{
			lock_guard<spin_mutex> lock(my_mutex);
			my_queue.emplace_front(std::forward<T>(task_func));
		}
		sem.post();
	}
	//清空任务列队
	void push_exit(unsigned int n) {
		sem.post(n);
	}
	//从列队获取一个任务，由执行线程执行
	bool get_task(function<void(void)> &tsk) {
		sem.wait();
		lock_guard<spin_mutex> lock(my_mutex);
		if (my_queue.size() == 0) {
			return false;
		}
		tsk = my_queue.front();
		my_queue.pop_front();
		return true;
	}
    uint64_t size() const{
        lock_guard<spin_mutex> lock(my_mutex);
        return my_queue.size();
    }
private:
	deque<function<void(void)>> my_queue;
	mutable spin_mutex my_mutex;
	semaphore sem;
};

} /* namespace Thread */
} /* namespace ZL */
#endif /* TASKQUEUE_H_ */
