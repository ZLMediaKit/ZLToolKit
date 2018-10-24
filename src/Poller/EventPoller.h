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

#ifndef EventPoller_h
#define EventPoller_h

#include <mutex>
#include <thread>
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include "PipeWrap.h"
#include "Util/logger.h"
#include "Util/util.h"
#include "Thread/List.h"
#include "Thread/TaskExecutor.h"

using namespace std;


#if defined(__linux__)
#define HAS_EPOLL
#endif //__linux__

namespace toolkit {

typedef enum {
	Event_Read = 1 << 0, //读事件
	Event_Write = 1 << 1, //写事件
	Event_Error = 1 << 2, //错误事件
	Event_LT    = 1 << 3,//水平触发
} Poll_Event;

typedef function<void(int event)> PollEventCB;
typedef function<void(bool success)> PollDelCB;


class EventPoller : public TaskExecutor , public std::enable_shared_from_this<EventPoller> {
public:
	typedef std::shared_ptr<EventPoller> Ptr;

	EventPoller();
	~EventPoller();
	static EventPoller &Instance();
	static void Destory();

	int addEvent(int fd, int event, const PollEventCB &eventCb);
	int delEvent(int fd, const PollDelCB &delCb = nullptr);
	int modifyEvent(int fd, int event);

	bool async(const TaskExecutor::Task &task, bool may_sync = true) override ;
    bool async_first(const TaskExecutor::Task &task, bool may_sync = true) override ;
	bool sync(const TaskExecutor::Task &task) override;
    bool sync_first(const TaskExecutor::Task &task) override;

	void runLoop(bool blocked = true);
	void shutdown() override;
	bool isMainThread();
    void wait() override ;
private:
	bool onPipeEvent();
    bool async_l(const TaskExecutor::Task &task, bool may_sync = true,bool first = false) ;
    bool sync_l(const TaskExecutor::Task &task,bool first = false);
private:
    class ExitException : public std::exception{
    public:
        ExitException(){}
        ~ExitException(){}
    };
private:
    PipeWrap _pipe;

	thread *_loopThread = nullptr;
	semaphore _sem_run_started;
	thread::id _mainThreadId;

    mutex _mtx_event_map;
#if defined(HAS_EPOLL)
	int _epoll_fd = -1;
	unordered_map<int, std::shared_ptr<PollEventCB> > _event_map;
#else
	struct Poll_Record{
		typedef std::shared_ptr<Poll_Record> Ptr;
		int event;
		int attach;
		PollEventCB callBack;
	};
	unordered_map<int, Poll_Record::Ptr > _event_map;
#endif //HAS_EPOLL

    mutex _mtx_runing;
    bool _loopRunned = false;

    List<TaskExecutor::Task> _list_task;
    mutex _mtx_task;
};


class EventPollerPool :
		public std::enable_shared_from_this<EventPollerPool> ,
		public TaskExecutorGetterImp {
public:
	typedef std::shared_ptr<EventPollerPool> Ptr;
	~EventPollerPool(){};

	static EventPollerPool &Instance();
	static void Destory();
	EventPoller::Ptr getFirstPoller();
	EventPoller::Ptr getPoller();
private:
	EventPollerPool() ;
};

}  // namespace toolkit

#endif /* EventPoller_h */









