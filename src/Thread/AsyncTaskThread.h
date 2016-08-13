//
//  AsyncTaskThread.h
//  quhuwai
//
//  Created by wzkj on 15/6/8.
//  Copyright (c) 2015年 wzkj. All rights reserved.
//

#ifndef __quhuwai__AsyncTaskThread__
#define __quhuwai__AsyncTaskThread__

#include <stdio.h>
#include <sys/time.h>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <memory>
#include <atomic>
using namespace std;
#define TASK_INTERVAL 100

namespace ZL {
namespace Thread {
typedef struct {
	uint64_t type;
	uint64_t timeLine;
	uint64_t tickTime;
	function<bool()> task;
} TaskInfo;

class AsyncTaskThread {
public:
	//the timer default 30s
	AsyncTaskThread(uint64_t millisecond_sleep = 1000 * 30);
	~AsyncTaskThread();
	void DoTaskDelay(uint64_t type, uint64_t millisecond, const function<bool()> &func);
	void CancelTask(uint64_t type);
	static AsyncTaskThread &Instance() {
		static AsyncTaskThread instance(TASK_INTERVAL);
		return instance;
	}
private:
	mutex _mtx;
	unordered_multimap<uint64_t, shared_ptr<TaskInfo> > taskMap;
	unordered_set<uint64_t> needCancel;
	inline uint64_t getNowTime();
	thread *taskThread;
	void DoTask();
	atomic_bool threadExit;
	condition_variable_any cond;
	uint64_t millisecond_sleep;
};

} /* namespace Thread */
} /* namespace ZL */

#endif /* defined(__quhuwai__AsyncTaskThread__) */
