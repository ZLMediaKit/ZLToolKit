/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef EventPoller_h
#define EventPoller_h

#include <mutex>
#include <thread>
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "PipeWrap.h"
#include "Util/logger.h"
#include "Util/List.h"
#include "Thread/TaskExecutor.h"
#include "Thread/ThreadPool.h"
#include "Network/Buffer.h"
#include "Network/BufferSock.h"

#if defined(__linux__) || defined(__linux)
#define HAS_EPOLL
#endif //__linux__

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define HAS_KQUEUE
#endif // __APPLE__

namespace toolkit {

class EventPoller : public TaskExecutor, public AnyStorage, public std::enable_shared_from_this<EventPoller> {
public:
    friend class TaskExecutorGetterImp;

    using Ptr = std::shared_ptr<EventPoller>;
    using PollEventCB = std::function<void(int event)>;
    using PollCompleteCB = std::function<void(bool success)>;
    using DelayTask = TaskCancelableImp<uint64_t(void)>;

    typedef enum {
        Event_Read = 1 << 0, //读事件
        Event_Write = 1 << 1, //写事件
        Event_Error = 1 << 2, //错误事件
        Event_LT = 1 << 3,//水平触发
    } Poll_Event;

    ~EventPoller();

    /**
     * 获取EventPollerPool单例中的第一个EventPoller实例，
     * 保留该接口是为了兼容老代码
     * @return 单例
     * Gets the first EventPoller instance from the EventPollerPool singleton,
     * This interface is preserved for compatibility with old code.
     * @return singleton
     
     * [AUTO-TRANSLATED:b536ebf6]
     */
    static EventPoller &Instance();

    /**
     * 添加事件监听
     * @param fd 监听的文件描述符
     * @param event 事件类型，例如 Event_Read | Event_Write
     * @param cb 事件回调functional
     * @return -1:失败，0:成功
     * Adds an event listener
     * @param fd The file descriptor to listen to
     * @param event The event type, e.g. Event_Read | Event_Write
     * @param cb The event callback function
     * @return -1: failed, 0: success
     
     * [AUTO-TRANSLATED:cfba4c75]
     */
    int addEvent(int fd, int event, PollEventCB cb);

    /**
     * 删除事件监听
     * @param fd 监听的文件描述符
     * @param cb 删除成功回调functional
     * @return -1:失败，0:成功
     * Deletes an event listener
     * @param fd The file descriptor to stop listening to
     * @param cb The callback function for successful deletion
     * @return -1: failed, 0: success
     
     * [AUTO-TRANSLATED:be6fdf51]
     */
    int delEvent(int fd, PollCompleteCB cb = nullptr);

    /**
     * 修改监听事件类型
     * @param fd 监听的文件描述符
     * @param event 事件类型，例如 Event_Read | Event_Write
     * @return -1:失败，0:成功
     * Modifies the event type being listened to
     * @param fd The file descriptor to modify
     * @param event The new event type, e.g. Event_Read | Event_Write
     * @return -1: failed, 0: success
     
     * [AUTO-TRANSLATED:becf3d09]
     */
    int modifyEvent(int fd, int event, PollCompleteCB cb = nullptr);

    /**
     * 异步执行任务
     * @param task 任务
     * @param may_sync 如果调用该函数的线程就是本对象的轮询线程，那么may_sync为true时就是同步执行任务
     * @return 是否成功，一定会返回true
     * Executes a task asynchronously
     * @param task The task to execute
     * @param may_sync If the calling thread is the polling thread of this object,
     *                  then if may_sync is true, the task will be executed synchronously
     * @return Whether the task was executed successfully (always returns true)
     
     * [AUTO-TRANSLATED:071f7ed8]
     */
    Task::Ptr async(TaskIn task, bool may_sync = true) override;

    /**
     * 同async方法，不过是把任务打入任务列队头，这样任务优先级最高
     * @param task 任务
     * @param may_sync 如果调用该函数的线程就是本对象的轮询线程，那么may_sync为true时就是同步执行任务
     * @return 是否成功，一定会返回true
     * Similar to async, but adds the task to the head of the task queue,
     * giving it the highest priority
     * @param task The task to execute
     * @param may_sync If the calling thread is the polling thread of this object,
     *                  then if may_sync is true, the task will be executed synchronously
     * @return Whether the task was executed successfully (always returns true)
     
     * [AUTO-TRANSLATED:9ef5169b]
     */
    Task::Ptr async_first(TaskIn task, bool may_sync = true) override;

    /**
     * 判断执行该接口的线程是否为本对象的轮询线程
     * @return 是否为本对象的轮询线程
     * Checks if the thread calling this interface is the polling thread of this object
     * @return Whether the calling thread is the polling thread
     
     * [AUTO-TRANSLATED:db9a4916]
     */
    bool isCurrentThread();

    /**
     * 延时执行某个任务
     * @param delay_ms 延时毫秒数
     * @param task 任务，返回值为0时代表不再重复任务，否则为下次执行延时，如果任务中抛异常，那么默认不重复任务
     * @return 可取消的任务标签
     * Delays the execution of a task
     * @param delay_ms The delay in milliseconds
     * @param task The task to execute, returns 0 to stop repeating the task,
     *              otherwise returns the delay for the next execution.
     *              If an exception is thrown in the task, it defaults to not repeating the task.
     * @return A cancellable task label
     
     * [AUTO-TRANSLATED:61f97e64]
     */
    DelayTask::Ptr doDelayTask(uint64_t delay_ms, std::function<uint64_t()> task);

    /**
     * 获取当前线程关联的Poller实例
     * Gets the Poller instance associated with the current thread
     
     * [AUTO-TRANSLATED:debcf0e2]
     */
    static EventPoller::Ptr getCurrentPoller();

    /**
     * 获取当前线程下所有socket共享的读缓存
     * Gets the shared read buffer for all sockets in the current thread
     
     * [AUTO-TRANSLATED:2796f458]
     */
    SocketRecvBuffer::Ptr getSharedBuffer(bool is_udp);

    /**
     * 获取poller线程id
     * Get the poller thread ID
     
     * [AUTO-TRANSLATED:1c968752]
     */
    std::thread::id getThreadId() const;

    /**
     * 获取线程名
     * Get the thread name
     
     * [AUTO-TRANSLATED:842652d9]
     */
    const std::string& getThreadName() const;

private:
    /**
     * 本对象只允许在EventPollerPool中构造
     * This object can only be constructed in EventPollerPool
     
     * [AUTO-TRANSLATED:0c9a8a28]
     */
    EventPoller(std::string name);

    /**
     * 执行事件轮询
     * @param blocked 是否用执行该接口的线程执行轮询
     * @param ref_self 是记录本对象到thread local变量
     * Perform event polling
     * @param blocked Whether to execute polling with the thread that calls this interface
     * @param ref_self Whether to record this object to thread local variable
     
     * [AUTO-TRANSLATED:b0ac803c]
     */
    void runLoop(bool blocked, bool ref_self);

    /**
     * 内部管道事件，用于唤醒轮询线程用
     * Internal pipe event, used to wake up the polling thread
     
     * [AUTO-TRANSLATED:022754b9]
     */
    void onPipeEvent(bool flush = false);

    /**
     * 切换线程并执行任务
     * @param task
     * @param may_sync
     * @param first
     * @return 可取消的任务本体，如果已经同步执行，则返回nullptr
     * Switch threads and execute tasks
     * @param task
     * @param may_sync
     * @param first
     * @return The cancellable task itself, or nullptr if it has been executed synchronously
     
     * [AUTO-TRANSLATED:e7019c4a]
     */
    Task::Ptr async_l(TaskIn task, bool may_sync = true, bool first = false);

    /**
     * 结束事件轮询
     * 需要指出的是，一旦结束就不能再次恢复轮询线程
     * End event polling
     * Note that once ended, the polling thread cannot be resumed
     
     * [AUTO-TRANSLATED:4f232154]
     */
    void shutdown();

    /**
     * 刷新延时任务
     * Refresh delayed tasks
     
     * [AUTO-TRANSLATED:88104b90]
     */
    int64_t flushDelayTask(uint64_t now);

    /**
     * 获取select或epoll休眠时间
     * Get the sleep time for select or epoll
     
     * [AUTO-TRANSLATED:34e0384e]
     */
    int64_t getMinDelay();

    /**
     * 添加管道监听事件
     * Add pipe listening event
     
     * [AUTO-TRANSLATED:06e5bc67]
     */
    void addEventPipe();

private:
    class ExitException : public std::exception {};

private:
    //标记loop线程是否退出  [AUTO-TRANSLATED:98250f84]
    //标记loop线程是否退出
// Mark the loop thread as exited
    bool _exit_flag;
    //线程名  [AUTO-TRANSLATED:f1d62d9f]
    //线程名
// Thread name
    std::string _name;
    //当前线程下，所有socket共享的读缓存  [AUTO-TRANSLATED:6ce70017]
    //当前线程下，所有socket共享的读缓存
// Shared read buffer for all sockets under the current thread
    std::weak_ptr<SocketRecvBuffer> _shared_buffer[2];
    //执行事件循环的线程  [AUTO-TRANSLATED:2465cc75]
    //执行事件循环的线程
// Thread that executes the event loop
    std::thread *_loop_thread = nullptr;
    //通知事件循环的线程已启动  [AUTO-TRANSLATED:61f478cf]
    //通知事件循环的线程已启动
// Notify the event loop thread that it has started
    semaphore _sem_run_started;

    //内部事件管道  [AUTO-TRANSLATED:dc1d3a93]
    //内部事件管道
// Internal event pipe
    PipeWrap _pipe;
    //从其他线程切换过来的任务  [AUTO-TRANSLATED:d16917d6]
    //从其他线程切换过来的任务
// Tasks switched from other threads
    std::mutex _mtx_task;
    List<Task::Ptr> _list_task;

    //保持日志可用  [AUTO-TRANSLATED:4a6c2438]
    //保持日志可用
// Keep the log available
    Logger::Ptr _logger;

#if defined(HAS_EPOLL) || defined(HAS_KQUEUE)
    // epoll和kqueue相关  [AUTO-TRANSLATED:84d2785e]
    //epoll和kqueue相关
// epoll and kqueue related
    int _event_fd = -1;
    std::unordered_map<int, std::shared_ptr<PollEventCB> > _event_map;
#else
    // select相关  [AUTO-TRANSLATED:bf3e2edd]
    //select相关
// select related
    struct Poll_Record {
        using Ptr = std::shared_ptr<Poll_Record>;
        int fd;
        int event;
        int attach;
        PollEventCB call_back;
    };
    std::unordered_map<int, Poll_Record::Ptr> _event_map;
#endif //HAS_EPOLL
    std::unordered_set<int> _event_cache_expired;

    //定时器相关  [AUTO-TRANSLATED:fa2e84da]
    //Timer related
    std::multimap<uint64_t, DelayTask::Ptr> _delay_task_map;
};

class EventPollerPool : public std::enable_shared_from_this<EventPollerPool>, public TaskExecutorGetterImp {
public:
    using Ptr = std::shared_ptr<EventPollerPool>;
    static const std::string kOnStarted;
    #define EventPollerPoolOnStartedArgs EventPollerPool &pool, size_t &size

    ~EventPollerPool() = default;

    /**
     * 获取单例
     * @return
     * Get singleton
     * @return
     
     * [AUTO-TRANSLATED:1cb32aa7]
     */
    static EventPollerPool &Instance();

    /**
     * 设置EventPoller个数，在EventPollerPool单例创建前有效
     * 在不调用此方法的情况下，默认创建thread::hardware_concurrency()个EventPoller实例
     * @param size  EventPoller个数，如果为0则为thread::hardware_concurrency()
     * Set the number of EventPoller instances, effective before the EventPollerPool singleton is created
     * If this method is not called, the default is to create thread::hardware_concurrency() EventPoller instances
     * @param size  Number of EventPoller instances, 0 means thread::hardware_concurrency()
     
     * [AUTO-TRANSLATED:bdc02181]
     */
    static void setPoolSize(size_t size = 0);

    /**
     * 内部创建线程是否设置cpu亲和性，默认设置cpu亲和性
     * Whether to set CPU affinity for internal thread creation, default is to set CPU affinity
     
     * [AUTO-TRANSLATED:46941c9f]
     */
    static void enableCpuAffinity(bool enable);

    /**
     * 获取第一个实例
     * @return
     * Get the first instance
     * @return
     
     * [AUTO-TRANSLATED:a76aad3b]
     */
    EventPoller::Ptr getFirstPoller();

    /**
     * 根据负载情况获取轻负载的实例
     * 如果优先返回当前线程，那么会返回当前线程
     * 返回当前线程的目的是为了提高线程安全性
     * @param prefer_current_thread 是否优先获取当前线程
     * Get a lightly loaded instance based on the load
     * If prioritizing the current thread, it will return the current thread
     * The purpose of returning the current thread is to improve thread safety
     * @param prefer_current_thread Whether to prioritize getting the current thread
     
     * [AUTO-TRANSLATED:f0830806]
     */
    EventPoller::Ptr getPoller(bool prefer_current_thread = true);

    /**
     * 设置 getPoller() 是否优先返回当前线程
     * 在批量创建Socket对象时，如果优先返回当前线程，
     * 那么将导致负载不够均衡，所以可以暂时关闭然后再开启
     * @param flag 是否优先返回当前线程
     * Set whether getPoller() prioritizes returning the current thread
     * When creating Socket objects in batches, if prioritizing the current thread,
     * it will cause the load to be unbalanced, so it can be temporarily closed and then reopened
     * @param flag Whether to prioritize returning the current thread
     
     * [AUTO-TRANSLATED:c354e1d5]
     */
    void preferCurrentThread(bool flag = true);

private:
    EventPollerPool();

private:
    bool _prefer_current_thread = true;
};

}  // namespace toolkit
#endif /* EventPoller_h */
