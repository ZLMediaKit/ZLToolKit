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

#ifndef THREADGROUP_H_
#define THREADGROUP_H_

#include <set>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_map>

using namespace std;

namespace ZL {
namespace Thread {

class thread_group {
private:
	thread_group(thread_group const&);
	thread_group& operator=(thread_group const&);
public:
	thread_group() {
	}
	~thread_group() {
		for (auto &th : threads) {
			delete th.second;
		}
	}

	bool is_this_thread_in() {
		auto it = threads.find(this_thread::get_id());
		return it != threads.end();
	}

	bool is_thread_in(thread* thrd) {
		if (thrd) {
			auto it = threads.find(thrd->get_id());
			return it != threads.end();
		} else {
			return false;
		}
	}

	template<typename F>
	thread* create_thread(F threadfunc) {
		thread  *new_thread=new thread(threadfunc);
		threads[new_thread->get_id()] = new_thread;
		return new_thread;
	}

	void add_thread(thread* thrd) {
		if (thrd) {
			if (is_thread_in(thrd)) {
				throw runtime_error(
						"thread_group: trying to add a duplicated thread");
			}
			threads[thrd->get_id()] = thrd;
		}
	}

	void remove_thread(thread* thrd) {
		auto it = threads.find(thrd->get_id());
		if (it != threads.end()) {
			threads.erase(it);
		}
	}
	void join_all() {
		if (is_this_thread_in()) {
			throw runtime_error("thread_group: trying joining itself");
			return;
		}
		for (auto &it : threads) {
			if (it.second->joinable()) {
				it.second->join(); //等待线程主动退出
			}
		}
	}
	size_t size() {
		return threads.size();
	}
private:
	unordered_map<thread::id, thread*> threads;

};

} /* namespace Thread */
} /* namespace ZL */
#endif /* THREADGROUP_H_ */
