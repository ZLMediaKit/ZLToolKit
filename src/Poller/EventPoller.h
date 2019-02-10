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
#include "Util/List.h"
#include "Thread/TaskExecutor.h"

using namespace std;


#if defined(__linux__) || defined(__linux)
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

class DelayTask {
public:
	typedef std::shared_ptr<DelayTask> Ptr;
	DelayTask(){}
	~DelayTask(){}

	/**
	 * 取消任务
	 * 在取消延时任务时，并不会立即销毁lambad表达式中的强引用，要在下一次tick时才会真正移除
	 */
	virtual void cancel() = 0;

	/**
	 * 执行任务
	 * @return
	 */
	virtual uint64_t operator()() const = 0;
};

class EventPoller : public TaskExecutor , public std::enable_shared_from_this<EventPoller> {
public:
	typedef std::shared_ptr<EventPoller> Ptr;
	friend class EventPollerPool;

	~EventPoller();


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
	 * 判断执行该接口的线程是否为本对象的轮询线程
	 * @return 是否为本对象的轮询线程
	 */
	bool isCurrentThread();

	/**
	 * 延时执行某个任务
	 * @param delayMS 延时毫秒数
	 * @param task 任务，返回值为0时代表不再重复任务，否则为下次执行延时，如果任务中抛异常，那么默认不重复任务
	 * @return 可取消的任务标签
	 */
	DelayTask::Ptr doDelayTask(uint64_t delayMS, const function<uint64_t()> &task);
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

	/**
     * 阻塞当前线程，等待轮询线程退出;
     * 在执行shutdown接口时本函数会退出
     */
	void wait() ;


	/**
     * 结束事件轮询
     * 需要指出的是，一旦结束就不能再次恢复轮询线程
     */
	void shutdown();

	/**
	 * 刷新延时任务
	 */
	uint64_t flushDelayTask();

	/**
	 * 获取select或epoll休眠时间
	 */
	uint64_t getMinDelay();
private:
    class ExitException : public std::exception{
    public:
        ExitException(){}
        ~ExitException(){}
    };

	class DelayTaskImp : public DelayTask{
	public:
		typedef std::shared_ptr<DelayTaskImp> Ptr;
		template <typename FUN>
		DelayTaskImp(FUN &&task) : _task(std::forward<FUN>(task)),_canceled(false){}
		~DelayTaskImp(){}
		void cancel() override {
			_canceled = true;
			//由于追求性能最大化，此处并未置空_task，
            //这样_task中捕获的强引用要在下一次tick时才会移除
		};
		uint64_t operator()() const override{
			if(_canceled){
				return 0;
			}
			return _task();
		}
	private:
		function<uint64_t()> _task;
		atomic_bool _canceled;
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
    multimap<uint64_t,DelayTaskImp::Ptr > _delayTask;
	uint64_t _minDelay = 0;

    mutex _mtx_task;
    Logger::Ptr _logger;
};


class EventPollerPool :
		public std::enable_shared_from_this<EventPollerPool> ,
		public TaskExecutorGetterImp {
public:
	typedef std::shared_ptr<EventPollerPool> Ptr;
	~EventPollerPool(){};

	/**
	 * 获取单例
	 * @return
	 */
	static EventPollerPool &Instance();

	/**
	 * 根据负载情况获取轻负载的实例
	 * @return
	 */
	EventPoller::Ptr getPoller();
private:
	EventPollerPool() ;
};

}  // namespace toolkit

#endif /* EventPoller_h */









