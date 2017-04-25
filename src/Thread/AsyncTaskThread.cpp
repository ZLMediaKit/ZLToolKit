//
//  AsyncTaskThread.cpp
//  xzl
//
//  Created by xzl on 15/6/8.
//

#include <iostream>
#include "Util/logger.h"
#include "AsyncTaskThread.h"

using namespace std;
using namespace ZL::Util;

namespace ZL {
namespace Thread {
AsyncTaskThread::AsyncTaskThread(uint64_t _millisecond_sleep) {
	millisecond_sleep = _millisecond_sleep;
	threadExit = false;
	taskThread = new thread(&AsyncTaskThread::DoTask, this);
}
AsyncTaskThread::~AsyncTaskThread() {
	if (taskThread != NULL) {
		threadExit = true;
		cond.notify_one();
		taskThread->join();
		delete taskThread;
		taskThread = NULL;
	}
	InfoL;
}

void AsyncTaskThread::DoTaskDelay(uint64_t type, uint64_t millisecond,const function<bool()> &func) {
	lock_guard<recursive_mutex> lck(_mtx);
	std::shared_ptr<TaskInfo> info(new TaskInfo);
	info->type = type;
	info->timeLine = millisecond + getNowTime();
	info->task = func;
	info->tickTime = millisecond;
	taskMap.emplace(type, info);
	needCancel.erase(type);
	if (taskMap.size() == 1) {
		cond.notify_one();
	}
}
void AsyncTaskThread::CancelTask(uint64_t type) {
	lock_guard<recursive_mutex> lck(_mtx);
	taskMap.erase(type);
	needCancel.emplace(type);
}
inline uint64_t AsyncTaskThread::getNowTime() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
void AsyncTaskThread::DoTask() {
	deque <std::shared_ptr<TaskInfo> > TaskNeedDo;
	while (!threadExit) {
		unique_lock<recursive_mutex> lck(_mtx);
		if (taskMap.size() != 0) {
			cond.wait_for(lck, chrono::milliseconds(millisecond_sleep));
		} else {
			cond.wait_for(lck, chrono::hours(24));
		}
		//获取需要执行的任务
		for (auto it = taskMap.begin(); it != taskMap.end();) {
			auto &info = it->second;
			uint64_t now = getNowTime();
			if (now > info->timeLine) {
				TaskNeedDo.push_back(info);
				it = taskMap.erase(it);
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
				FatalL << ex.what();
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
			if (needCancel.find(task->type) == needCancel.end()) {
				taskMap.emplace(task->type, task);
			}
		}
		needCancel.clear();
		TaskNeedDo.clear();
	}
}
} /* namespace Thread */
} /* namespace ZL */
