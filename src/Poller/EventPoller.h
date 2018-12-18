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
using namespace ZL::Util;
using namespace ZL::Thread;


#if defined(__linux__)
#define HAS_EPOLL
#endif //__linux__

namespace ZL {
namespace Poller {

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
	friend class EventPollerPool;

	~EventPoller();

	/**
	 * 获取EventPollerPool单例中的第一个EventPoller实例，
	 * 保留该接口是为了兼容老代码
	 * @return 单例
	 */
	static EventPoller &Instance();

	/**
	 * 销毁EventPollerPool单例，等同于EventPollerPool::Destory(),
	 * 保留该接口是为了兼容老代码
	 */
	static void Destory();

	/**
	 * 添加事件监听
	 * @param fd 监听的文件描述符
	 * @param event 事件类型，例如 Event_Read | Event_Write
	 * @param eventCb 事件回调functional
	 * @return -1:失败，0:成功
	 */
	int addEvent(int fd, int event, const PollEventCB &eventCb);

	/**
	 * 删除事件监听
	 * @param fd 监听的文件描述符
	 * @param delCb 删除成功回调functional
	 * @return -1:失败，0:成功
	 */
	int delEvent(int fd, const PollDelCB &delCb = nullptr);

	/**
	 * 修改监听事件类型
	 * @param fd 监听的文件描述符
	 * @param event 事件类型，例如 Event_Read | Event_Write
	 * @return -1:失败，0:成功
	 */
	int modifyEvent(int fd, int event);


	/**
	 * 异步执行任务
	 * @param task 任务
	 * @param may_sync 如果调用该函数的线程就是本对象的轮询线程，那么may_sync为true时就是同步执行任务
	 * @return 是否成功，一定会返回true
	 */
	bool async(const TaskExecutor::Task &task, bool may_sync = true) override ;

	/**
	 * 同async方法，不过是把任务打入任务列队头，这样任务优先级最高
	 * @param task 任务
	 * @param may_sync 如果调用该函数的线程就是本对象的轮询线程，那么may_sync为true时就是同步执行任务
	 * @return 是否成功，一定会返回true
	 */
    bool async_first(const TaskExecutor::Task &task, bool may_sync = true) override ;

    /**
     * 在轮询线程中执行任务并且等待其执行结束
     * @param task 任务
     * @return 是否成功，一定会返回true
     */
	bool sync(const TaskExecutor::Task &task) override;

	/**
    * 同sync方法，不过是把任务打入任务列队头，这样任务优先级最高
    * @param task 任务
    * @return 是否成功，一定会返回true
    */
    bool sync_first(const TaskExecutor::Task &task) override;

    /**
     * 在blocked时则等待轮询线程退出，功能相当于wait接口
     * 否则什么也不干
     * 保留本接口的目的是为了兼容老代码
     * 老接口的原有功能已经被runLoopOnce接口替代
     * @param blocked 是否等待轮询线程退出
     */
	void runLoop(bool blocked = true);

	/**
	 * 结束事件轮询
	 * 需要指出的是，一旦结束就不能再次恢复轮询线程
	 */
	void shutdown() override;

	/**
	 * 判断执行该接口的线程是否为本对象的轮询线程
	 * @return 是否为本对象的轮询线程
	 */
	bool isMainThread();

	/**
	 * 阻塞当前线程，等待轮询线程退出;
	 * 在执行shutdown接口时本函数会退出
	 */
    void wait() override ;
private:
	/**
	 * 本对象只允许在EventPollerPool中构造
	 */
	EventPoller();

	/**
	 * 执行事件轮询
	 * @param blocked 是否用执行该接口的线程执行轮询
	 */
	void runLoopOnce(bool blocked = true);

	/**
	 * 内部管道事件，用于唤醒轮询线程用
	 * @return
	 */
	bool onPipeEvent();

	/**
	 * 切换线程并执行任务
	 * @param task
	 * @param may_sync
	 * @param first
	 * @return
	 */
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

	/**
	 * 获取第一个实例
	 * @return
	 */
	EventPoller::Ptr getFirstPoller();

	/**
	 * 根据负载情况获取轻负载的实例
	 * @return
	 */
	EventPoller::Ptr getPoller();
private:
	EventPollerPool() ;
};



}  // namespace Poller
}  // namespace ZL
#endif /* EventPoller_h */









