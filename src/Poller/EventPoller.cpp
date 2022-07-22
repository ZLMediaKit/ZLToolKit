/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "SelectWrap.h"
#include "EventPoller.h"
#include "Util/util.h"
#include "Util/uv_errno.h"
#include "Util/TimeTicker.h"
#include "Network/sockutil.h"

#if defined(HAS_EPOLL)
#include <sys/epoll.h>

#if !defined(EPOLLEXCLUSIVE)
#define EPOLLEXCLUSIVE 0
#endif

#define EPOLL_SIZE 1024

//防止epoll惊群
#ifndef EPOLLEXCLUSIVE
#define EPOLLEXCLUSIVE 0
#endif

#define toEpoll(event)        (((event) & Event_Read)  ? EPOLLIN : 0) \
                            | (((event) & Event_Write) ? EPOLLOUT : 0) \
                            | (((event) & Event_Error) ? (EPOLLHUP | EPOLLERR) : 0) \
                            | (((event) & Event_LT)    ? 0 : EPOLLET)

#define toPoller(epoll_event)     (((epoll_event) & EPOLLIN) ? Event_Read   : 0) \
                                | (((epoll_event) & EPOLLOUT) ? Event_Write : 0) \
                                | (((epoll_event) & EPOLLHUP) ? Event_Error : 0) \
                                | (((epoll_event) & EPOLLERR) ? Event_Error : 0)
#endif //HAS_EPOLL

using namespace std;

namespace toolkit {

EventPoller &EventPoller::Instance() {
    return *(EventPollerPool::Instance().getFirstPoller());
}

EventPoller::EventPoller(ThreadPool::Priority priority) {
    _priority = priority;
    SockUtil::setNoBlocked(_pipe.readFD());
    SockUtil::setNoBlocked(_pipe.writeFD());

#if defined(HAS_EPOLL)
    _epoll_fd = epoll_create(EPOLL_SIZE);
    if (_epoll_fd == -1) {
        throw runtime_error(StrPrinter << "创建epoll文件描述符失败:" << get_uv_errmsg());
    }
    SockUtil::setCloExec(_epoll_fd);
#endif //HAS_EPOLL
    _logger = Logger::Instance().shared_from_this();
    _loop_thread_id = this_thread::get_id();

    //添加内部管道事件
    if (addEvent(_pipe.readFD(), Event_Read, [this](int event) { onPipeEvent(); }) == -1) {
        throw std::runtime_error("epoll添加管道失败");
    }
}

void EventPoller::shutdown() {
    async_l([]() {
        throw ExitException();
    }, false, true);

    if (_loop_thread) {
        //防止作为子进程时崩溃
        try { _loop_thread->join(); } catch (...) {}
        delete _loop_thread;
        _loop_thread = nullptr;
    }
}

EventPoller::~EventPoller() {
    shutdown();
    wait();
#if defined(HAS_EPOLL)
    if (_epoll_fd != -1) {
        close(_epoll_fd);
        _epoll_fd = -1;
    }
#endif //defined(HAS_EPOLL)
    //退出前清理管道中的数据
    _loop_thread_id = this_thread::get_id();
    onPipeEvent();
    InfoL << this;
}

int EventPoller::addEvent(int fd, int event, PollEventCB cb) {
    TimeTicker();
    if (!cb) {
        WarnL << "PollEventCB 为空!";
        return -1;
    }

    if (isCurrentThread()) {
#if defined(HAS_EPOLL)
        struct epoll_event ev = {0};
        ev.events = (toEpoll(event)) | EPOLLEXCLUSIVE;
        ev.data.fd = fd;
        int ret = epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &ev);
        if (ret == 0) {
            _event_map.emplace(fd, std::make_shared<PollEventCB>(std::move(cb)));
        }
        return ret;
#else
#ifndef _WIN32
        //win32平台，socket套接字不等于文件描述符，所以可能不适用这个限制
        if (fd >= FD_SETSIZE || _event_map.size() >= FD_SETSIZE) {
            WarnL << "select最多监听" << FD_SETSIZE << "个文件描述符";
            return -1;
        }
#endif
        auto record = std::make_shared<Poll_Record>();
        record->event = event;
        record->call_back = std::move(cb);
        _event_map.emplace(fd, record);
        return 0;
#endif //HAS_EPOLL
    }

    async([this, fd, event, cb]() {
        addEvent(fd, event, std::move(const_cast<PollEventCB &>(cb)));
    });
    return 0;
}

int EventPoller::delEvent(int fd, PollDelCB cb) {
    TimeTicker();
    if (!cb) {
        cb = [](bool success) {};
    }

    if (isCurrentThread()) {
#if defined(HAS_EPOLL)
        bool success = epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, nullptr) == 0 && _event_map.erase(fd) > 0;
        cb(success);
        return success ? 0 : -1;
#else
        cb(_event_map.erase(fd));
        return 0;
#endif //HAS_EPOLL

    }

    //跨线程操作
    async([this, fd, cb]() {
        delEvent(fd, std::move(const_cast<PollDelCB &>(cb)));
    });
    return 0;
}

int EventPoller::modifyEvent(int fd, int event) {
    TimeTicker();
#if defined(HAS_EPOLL)
    struct epoll_event ev = {0};
    ev.events = toEpoll(event);
    ev.data.fd = fd;
    return epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &ev);
#else
    if (isCurrentThread()) {
        auto it = _event_map.find(fd);
        if (it != _event_map.end()) {
            it->second->event = event;
        }
        return 0;
    }
    async([this, fd, event]() {
        modifyEvent(fd, event);
    });
    return 0;
#endif //HAS_EPOLL
}

Task::Ptr EventPoller::async(TaskIn task, bool may_sync) {
    return async_l(std::move(task), may_sync, false);
}

Task::Ptr EventPoller::async_first(TaskIn task, bool may_sync) {
    return async_l(std::move(task), may_sync, true);
}

Task::Ptr EventPoller::async_l(TaskIn task, bool may_sync, bool first) {
    TimeTicker();
    if (may_sync && isCurrentThread()) {
        task();
        return nullptr;
    }

    auto ret = std::make_shared<Task>(std::move(task));
    {
        lock_guard<mutex> lck(_mtx_task);
        if (first) {
            _list_task.emplace_front(ret);
        } else {
            _list_task.emplace_back(ret);
        }
    }
    //写数据到管道,唤醒主线程
    _pipe.write("", 1);
    return ret;
}

bool EventPoller::isCurrentThread() {
    return _loop_thread_id == this_thread::get_id();
}

inline void EventPoller::onPipeEvent() {
    char buf[1024];
    int err = 0;
    do {
        if (_pipe.read(buf, sizeof(buf)) > 0) {
            continue;
        }
        err = get_uv_error(true);
    } while (err != UV_EAGAIN);

    decltype(_list_task) _list_swap;
    {
        lock_guard<mutex> lck(_mtx_task);
        _list_swap.swap(_list_task);
    }

    _list_swap.for_each([&](const Task::Ptr &task) {
        try {
            (*task)();
        } catch (ExitException &) {
            _exit_flag = true;
        } catch (std::exception &ex) {
            ErrorL << "EventPoller执行异步任务捕获到异常:" << ex.what();
        }
    });
}

void EventPoller::wait() {
    lock_guard<mutex> lck(_mtx_running);
}

BufferRaw::Ptr EventPoller::getSharedBuffer() {
    auto ret = _shared_buffer.lock();
    if (!ret) {
        //预留一个字节存放\0结尾符
        ret = BufferRaw::create();
        ret->setCapacity(1 + SOCKET_DEFAULT_BUF_SIZE);
        _shared_buffer = ret;
    }
    return ret;
}

const thread::id &EventPoller::getThreadId() const {
    return _loop_thread_id;
}

static thread_local std::weak_ptr<EventPoller> s_current_poller;

// static
EventPoller::Ptr EventPoller::getCurrentPoller() {
    return s_current_poller.lock();
}

void EventPoller::runLoop(bool blocked, bool ref_self) {
    if (blocked) {
        ThreadPool::setPriority(_priority);
        lock_guard<mutex> lck(_mtx_running);
        _loop_thread_id = this_thread::get_id();
        if (ref_self) {
            s_current_poller = shared_from_this();
        }
        _sem_run_started.post();
        _exit_flag = false;
        uint64_t minDelay;
#if defined(HAS_EPOLL)
        struct epoll_event events[EPOLL_SIZE];
        while (!_exit_flag) {
            minDelay = getMinDelay();
            startSleep();//用于统计当前线程负载情况
            int ret = epoll_wait(_epoll_fd, events, EPOLL_SIZE, minDelay ? minDelay : -1);
            sleepWakeUp();//用于统计当前线程负载情况
            if (ret <= 0) {
                //超时或被打断
                continue;
            }
            for (int i = 0; i < ret; ++i) {
                struct epoll_event &ev = events[i];
                int fd = ev.data.fd;
                auto it = _event_map.find(fd);
                if (it == _event_map.end()) {
                    epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                    continue;
                }
                auto cb = it->second;
                try {
                    (*cb)(toPoller(ev.events));
                } catch (std::exception &ex) {
                    ErrorL << "EventPoller执行事件回调捕获到异常:" << ex.what();
                }
            }
        }
#else
        int ret, max_fd;
        FdSet set_read, set_write, set_err;
        List<Poll_Record::Ptr> callback_list;
        struct timeval tv;
        while (!_exit_flag) {
            //定时器事件中可能操作_event_map
            minDelay = getMinDelay();
            tv.tv_sec = (decltype(tv.tv_sec)) (minDelay / 1000);
            tv.tv_usec = 1000 * (minDelay % 1000);

            set_read.fdZero();
            set_write.fdZero();
            set_err.fdZero();
            max_fd = 0;
            for (auto &pr : _event_map) {
                if (pr.first > max_fd) {
                    max_fd = pr.first;
                }
                if (pr.second->event & Event_Read) {
                    set_read.fdSet(pr.first);//监听管道可读事件
                }
                if (pr.second->event & Event_Write) {
                    set_write.fdSet(pr.first);//监听管道可写事件
                }
                if (pr.second->event & Event_Error) {
                    set_err.fdSet(pr.first);//监听管道错误事件
                }
            }

            startSleep();//用于统计当前线程负载情况
            ret = zl_select(max_fd + 1, &set_read, &set_write, &set_err, minDelay ? &tv : nullptr);
            sleepWakeUp();//用于统计当前线程负载情况

            if (ret <= 0) {
                //超时或被打断
                continue;
            }
            //收集select事件类型
            for (auto &pr : _event_map) {
                int event = 0;
                if (set_read.isSet(pr.first)) {
                    event |= Event_Read;
                }
                if (set_write.isSet(pr.first)) {
                    event |= Event_Write;
                }
                if (set_err.isSet(pr.first)) {
                    event |= Event_Error;
                }
                if (event != 0) {
                    pr.second->attach = event;
                    callback_list.emplace_back(pr.second);
                }
            }

            callback_list.for_each([](Poll_Record::Ptr &record) {
                try {
                    record->call_back(record->attach);
                } catch (std::exception &ex) {
                    ErrorL << "EventPoller执行事件回调捕获到异常:" << ex.what();
                }
            });
            callback_list.clear();
        }
#endif //HAS_EPOLL
    } else {
        _loop_thread = new thread(&EventPoller::runLoop, this, true, ref_self);
        _sem_run_started.wait();
    }
}

uint64_t EventPoller::flushDelayTask(uint64_t now_time) {
    decltype(_delay_task_map) task_copy;
    task_copy.swap(_delay_task_map);

    for (auto it = task_copy.begin(); it != task_copy.end() && it->first <= now_time; it = task_copy.erase(it)) {
        //已到期的任务
        try {
            auto next_delay = (*(it->second))();
            if (next_delay) {
                //可重复任务,更新时间截止线
                _delay_task_map.emplace(next_delay + now_time, std::move(it->second));
            }
        } catch (std::exception &ex) {
            ErrorL << "EventPoller执行延时任务捕获到异常:" << ex.what();
        }
    }

    task_copy.insert(_delay_task_map.begin(), _delay_task_map.end());
    task_copy.swap(_delay_task_map);

    auto it = _delay_task_map.begin();
    if (it == _delay_task_map.end()) {
        //没有剩余的定时器了
        return 0;
    }
    //最近一个定时器的执行延时
    return it->first - now_time;
}

uint64_t EventPoller::getMinDelay() {
    auto it = _delay_task_map.begin();
    if (it == _delay_task_map.end()) {
        //没有剩余的定时器了
        return 0;
    }
    auto now = getCurrentMillisecond();
    if (it->first > now) {
        //所有任务尚未到期
        return it->first - now;
    }
    //执行已到期的任务并刷新休眠延时
    return flushDelayTask(now);
}

EventPoller::DelayTask::Ptr EventPoller::doDelayTask(uint64_t delay_ms, function<uint64_t()> task) {
    DelayTask::Ptr ret = std::make_shared<DelayTask>(std::move(task));
    auto time_line = getCurrentMillisecond() + delay_ms;
    async_first([time_line, ret, this]() {
        //异步执行的目的是刷新select或epoll的休眠时间
        _delay_task_map.emplace(time_line, ret);
    });
    return ret;
}

///////////////////////////////////////////////

static size_t s_pool_size = 0;
static bool s_enable_cpu_affinity = true;

INSTANCE_IMP(EventPollerPool)

EventPoller::Ptr EventPollerPool::getFirstPoller() {
    return dynamic_pointer_cast<EventPoller>(_threads.front());
}

EventPoller::Ptr EventPollerPool::getPoller(bool prefer_current_thread) {
    auto poller = EventPoller::getCurrentPoller();
    if (prefer_current_thread && _prefer_current_thread && poller) {
        return poller;
    }
    return dynamic_pointer_cast<EventPoller>(getExecutor());
}

void EventPollerPool::preferCurrentThread(bool flag) {
    _prefer_current_thread = flag;
}

EventPollerPool::EventPollerPool() {
    auto size = addPoller("event poller", s_pool_size, ThreadPool::PRIORITY_HIGHEST, true, s_enable_cpu_affinity);
    InfoL << "创建EventPoller个数:" << size;
}

void EventPollerPool::setPoolSize(size_t size) {
    s_pool_size = size;
}

void EventPollerPool::enableCpuAffinity(bool enable) {
    s_enable_cpu_affinity = enable;
}

}  // namespace toolkit

