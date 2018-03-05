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

#ifndef AsyncTaskThread_h
#define AsyncTaskThread_h

#include <stdio.h>
#include <deque>
#include <mutex>
#include <atomic>
#include <thread>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <condition_variable>
#include <functional>
#include "Util/util.h"

using namespace std;
using namespace ZL::Util;

#define TASK_INTERVAL 50

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
	AsyncTaskThread(uint64_t millisecond_sleep);
	~AsyncTaskThread();
	void DoTaskDelay(uint64_t type, uint64_t millisecond, const function<bool()> &func);
	void CancelTask(uint64_t type);
	static AsyncTaskThread &Instance(uint32_t millisecond_sleep = TASK_INTERVAL) {
		static AsyncTaskThread *instance(new AsyncTaskThread(millisecond_sleep));
		return *instance;
	}
	static void Destory(){
		delete &AsyncTaskThread::Instance();
	}
private:
	recursive_mutex _mtx;
	unordered_multimap<uint64_t, std::shared_ptr<TaskInfo> > taskMap;
	unordered_set<uint64_t> needCancel;
	inline uint64_t getNowTime();
	thread *taskThread;
	void DoTask();
	atomic_bool threadExit;
	condition_variable_any cond;
	uint64_t millisecond_sleep;
};

class AsyncTaskHelper
{
public:
	AsyncTaskHelper(uint64_t millisecond,const function<bool()> &task){
		AsyncTaskThread::Instance().DoTaskDelay(reinterpret_cast<uint64_t>(this),millisecond,task);
	}
	virtual ~AsyncTaskHelper(){
		AsyncTaskThread::Instance().CancelTask(reinterpret_cast<uint64_t>(this));
	}
};

} /* namespace Thread */
} /* namespace ZL */

#endif /* defined(AsyncTaskThread_h) */
