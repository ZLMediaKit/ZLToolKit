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

#include <iostream>
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "AsyncTaskThread.h"

using namespace std;

namespace toolkit {

static std::shared_ptr<AsyncTaskThread> s_instance;

AsyncTaskThread &AsyncTaskThread::Instance(uint32_t millisecond_sleep) {
	static onceToken s_token([&](){
		s_instance = std::make_shared<AsyncTaskThread>(millisecond_sleep);
	});
	return *s_instance;
}
void AsyncTaskThread::Destory(){
	s_instance.reset();
}

AsyncTaskThread::AsyncTaskThread(uint64_t millisecond_sleep) {
	_millisecond_sleep = millisecond_sleep;
	_threadExit = false;
	_taskThread = new thread(&AsyncTaskThread::DoTask, this);
}
AsyncTaskThread::~AsyncTaskThread() {
	if (_taskThread != NULL) {
		_threadExit = true;
		_cond.notify_one();
		_taskThread->join();
		delete _taskThread;
		_taskThread = NULL;
	}
	//InfoL;
}

void AsyncTaskThread::DoTaskDelay(uint64_t type, uint64_t millisecond,const function<bool()> &func) {
	lock_guard<recursive_mutex> lck(_mtx);
	std::shared_ptr<TaskInfo> info = std::make_shared<TaskInfo>();
	info->type = type;
	info->timeLine = millisecond + getNowTime();
	info->task = func;
	info->tickTime = millisecond;
	_taskMap.emplace(type, info);
	_needCancel.erase(type);
	if (_taskMap.size() == 1) {
		_cond.notify_one();
	}
}
void AsyncTaskThread::CancelTask(uint64_t type) {
	lock_guard<recursive_mutex> lck(_mtx);
	_taskMap.erase(type);
	_needCancel.emplace(type);
}
inline uint64_t AsyncTaskThread::getNowTime() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
void AsyncTaskThread::DoTask() {
	deque <std::shared_ptr<TaskInfo> > TaskNeedDo;
	while (!_threadExit) {
		unique_lock<recursive_mutex> lck(_mtx);
		if (_taskMap.size() != 0) {
			_cond.wait_for(lck, chrono::milliseconds(_millisecond_sleep));
		} else {
			_cond.wait_for(lck, chrono::hours(24));
		}
		//获取需要执行的任务
		for (auto it = _taskMap.begin(); it != _taskMap.end();) {
			auto &info = it->second;
			uint64_t now = getNowTime();
			if (now > info->timeLine) {
				TaskNeedDo.push_back(info);
				it = _taskMap.erase(it);
				continue;
			}
			it++;
		}
		_mtx.unlock();
		//unlock the mutex when do the timer task
		//执行任务
		for (auto it = TaskNeedDo.begin(); it != TaskNeedDo.end();) {
			auto &info = *it;
			bool flag = false;
			try {
				flag = info->task();
			} catch (std::exception &ex) {
				ErrorL << "catch exception:" << ex.what();
			}
			if (!flag) {
				it = TaskNeedDo.erase(it);
				continue;
			}
			info->timeLine = getNowTime() + info->tickTime;
			it++;
		}
		//relock the mutex
		_mtx.lock();
		//循环任务
		for (auto &task : TaskNeedDo) {
			if (_needCancel.find(task->type) == _needCancel.end()) {
				_taskMap.emplace(task->type, task);
			}
		}
		_needCancel.clear();
		TaskNeedDo.clear();
	}
}
} /* namespace toolkit */
