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

#ifndef THREADGROUP_H_
#define THREADGROUP_H_

#include <set>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_map>

using namespace std;

namespace toolkit {

class thread_group {
private:
	thread_group(thread_group const&);
	thread_group& operator=(thread_group const&);
public:
	thread_group() {
	}
	~thread_group() {
		_threads.clear();
	}

	bool is_this_thread_in() {
		auto thread_id = this_thread::get_id();
		if(_thread_id == thread_id){
			return true;
		}
		return _threads.find(thread_id) != _threads.end();
	}

	bool is_thread_in(thread* thrd) {
		if (!thrd) {
			return false;
		}
		auto it = _threads.find(thrd->get_id());
		return it != _threads.end();
	}

	template<typename F>
	thread* create_thread(F &&threadfunc) {
		auto thread_new = std::make_shared<thread>(threadfunc);
		_thread_id = thread_new->get_id();
		_threads[_thread_id] = thread_new;
		return thread_new.get();
	}

	void remove_thread(thread* thrd) {
		auto it = _threads.find(thrd->get_id());
		if (it != _threads.end()) {
			_threads.erase(it);
		}
	}
	void join_all() {
		if (is_this_thread_in()) {
			throw runtime_error("thread_group: trying joining itself");
		}
		for (auto &it : _threads) {
			if (it.second->joinable()) {
				it.second->join(); //等待线程主动退出
			}
		}
		_threads.clear();
	}
	size_t size() {
		return _threads.size();
	}
private:
	unordered_map<thread::id, std::shared_ptr<thread> > _threads;
	thread::id _thread_id;
};

} /* namespace toolkit */

#endif /* THREADGROUP_H_ */
