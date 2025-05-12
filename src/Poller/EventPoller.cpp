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
#include "Util/NoticeCenter.h"
#include "Network/sockutil.h"

#if defined(HAS_EPOLL)
#include <sys/epoll.h>

#if !defined(EPOLLEXCLUSIVE)
#define EPOLLEXCLUSIVE 0
#endif

#define EPOLL_SIZE 1024

//防止epoll惊群  [AUTO-TRANSLATED:ad53c775]
//Prevent epoll thundering
#ifndef EPOLLEXCLUSIVE
#define EPOLLEXCLUSIVE 0
#endif

#define toEpoll(event)        (((event) & Event_Read)  ? EPOLLIN : 0) \
                            | (((event) & Event_Write) ? EPOLLOUT : 0) \
                            | (((event) & Event_Error) ? (EPOLLHUP | EPOLLERR) : 0) \
                            | (((event) & Event_LT)    ? 0 : EPOLLET)

#define toPoller(epoll_event)     (((epoll_event) & (EPOLLIN | EPOLLRDNORM | EPOLLHUP)) ? Event_Read   : 0) \
                                | (((epoll_event) & (EPOLLOUT | EPOLLWRNORM)) ? Event_Write : 0) \
                                | (((epoll_event) & EPOLLHUP) ? Event_Error : 0) \
                                | (((epoll_event) & EPOLLERR) ? Event_Error : 0)
#define create_event() epoll_create(EPOLL_SIZE)
#endif //HAS_EPOLL

#if defined(HAS_KQUEUE)
#include <sys/event.h>
#define KEVENT_SIZE 1024
#define create_event() kqueue()
#endif // HAS_KQUEUE

using namespace std;

namespace toolkit {

EventPoller &EventPoller::Instance() {
    return *(EventPollerPool::Instance().getFirstPoller());
}

void EventPoller::addEventPipe() {
    SockUtil::setNoBlocked(_pipe.readFD());
    SockUtil::setNoBlocked(_pipe.writeFD());

    // 添加内部管道事件  [AUTO-TRANSLATED:6a72e39a]
    //Add internal pipe event
    if (addEvent(_pipe.readFD(), EventPoller::Event_Read, [this](int event) { onPipeEvent(); }) == -1) {
        throw std::runtime_error("Add pipe fd to poller failed");
    }
}

EventPoller::EventPoller(std::string name) {
#if defined(HAS_EPOLL) || defined(HAS_KQUEUE)
    _event_fd = create_event();
    if (_event_fd == -1) {
        throw runtime_error(StrPrinter << "Create event fd failed: " << get_uv_errmsg());
    }
    SockUtil::setCloExec(_event_fd);
#endif //HAS_EPOLL

    _name = std::move(name);
    _logger = Logger::Instance().shared_from_this();
    addEventPipe();
}

void EventPoller::shutdown() {
    async_l([]() {
        throw ExitException();
    }, false, true);

    if (_loop_thread) {
        //防止作为子进程时崩溃  [AUTO-TRANSLATED:68727e34]
        //Prevent crash when running as a child process
        try { _loop_thread->join(); } catch (...) { _loop_thread->detach(); }
        delete _loop_thread;
        _loop_thread = nullptr;
    }
}

EventPoller::~EventPoller() {
    shutdown();
    
#if defined(HAS_EPOLL) || defined(HAS_KQUEUE)
    if (_event_fd != -1) {
        close(_event_fd);
        _event_fd = -1;
    }
#endif

    //退出前清理管道中的数据  [AUTO-TRANSLATED:60e26f9a]
    //Clean up pipe data before exiting
    onPipeEvent(true);
    InfoL << getThreadName();
}

int EventPoller::addEvent(int fd, int event, PollEventCB cb) {
    TimeTicker();
    if (!cb) {
        WarnL << "PollEventCB is empty";
        return -1;
    }

    if (isCurrentThread()) {
#if defined(HAS_EPOLL)
        struct epoll_event ev = {0};
        ev.events = toEpoll(event) ;
        ev.data.fd = fd;
        int ret = epoll_ctl(_event_fd, EPOLL_CTL_ADD, fd, &ev);
        if (ret != -1) {
            _event_map.emplace(fd, std::make_shared<PollEventCB>(std::move(cb)));
        }
        return ret;
#elif defined(HAS_KQUEUE)
        struct kevent kev[2];
        int index = 0;
        if (event & Event_Read) {
            EV_SET(&kev[index++], fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, nullptr);
        }
        if (event & Event_Write) {
            EV_SET(&kev[index++], fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, nullptr);
        }
        int ret = kevent(_event_fd, kev, index, nullptr, 0, nullptr);
        if (ret != -1) {
            _event_map.emplace(fd, std::make_shared<PollEventCB>(std::move(cb)));
        }
        return ret;
#else
#ifndef _WIN32
        // win32平台，socket套接字不等于文件描述符，所以可能不适用这个限制  [AUTO-TRANSLATED:6adfc664]
        //On the win32 platform, the socket does not equal the file descriptor, so this restriction may not apply
        if (fd >= FD_SETSIZE) {
            WarnL << "select() can not watch fd bigger than " << FD_SETSIZE;
            return -1;
        }
#endif
        auto record = std::make_shared<Poll_Record>();
        record->fd = fd;
        record->event = event;
        record->call_back = std::move(cb);
        _event_map.emplace(fd, record);
        return 0;
#endif
    }

    async([this, fd, event, cb]() mutable {
        addEvent(fd, event, std::move(cb));
    });
    return 0;
}

int EventPoller::delEvent(int fd, PollCompleteCB cb) {
    TimeTicker();
    if (!cb) {
        cb = [](bool success) {};
    }

    if (isCurrentThread()) {
#if defined(HAS_EPOLL)
        int ret = -1;
        if (_event_map.erase(fd)) {
            _event_cache_expired.emplace(fd);
            ret = epoll_ctl(_event_fd, EPOLL_CTL_DEL, fd, nullptr);
        }
        cb(ret != -1);
        return ret;
#elif defined(HAS_KQUEUE)
        int ret = -1;
        if (_event_map.erase(fd)) {
            _event_cache_expired.emplace(fd);
            struct kevent kev[2];
            int index = 0;
            EV_SET(&kev[index++], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
            EV_SET(&kev[index++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
            ret = kevent(_event_fd, kev, index, nullptr, 0, nullptr);
        }
        cb(ret != -1);
        return ret;
#else
        int ret = -1;
        if (_event_map.erase(fd)) {
            _event_cache_expired.emplace(fd);
            ret = 0;
        }
        cb(ret != -1);
        return ret;
#endif //HAS_EPOLL
    }

    //跨线程操作  [AUTO-TRANSLATED:4e116519]
    //Cross-thread operation
    async([this, fd, cb]() mutable {
        delEvent(fd, std::move(cb));
    });
    return 0;
}

int EventPoller::modifyEvent(int fd, int event, PollCompleteCB cb) {
    TimeTicker();
    if (!cb) {
        cb = [](bool success) {};
    }
    if (isCurrentThread()) {
#if defined(HAS_EPOLL)
        struct epoll_event ev = { 0 };
        ev.events = toEpoll(event);
        ev.data.fd = fd;
        auto ret = epoll_ctl(_event_fd, EPOLL_CTL_MOD, fd, &ev);
        cb(ret != -1);
        return ret;
#elif defined(HAS_KQUEUE)
        struct kevent kev[2];
        int index = 0;
        EV_SET(&kev[index++], fd, EVFILT_READ, event & Event_Read ? EV_ADD | EV_CLEAR : EV_DELETE, 0, 0, nullptr);
        EV_SET(&kev[index++], fd, EVFILT_WRITE, event & Event_Write ? EV_ADD | EV_CLEAR : EV_DELETE, 0, 0, nullptr);
        int ret = kevent(_event_fd, kev, index, nullptr, 0, nullptr);
        cb(ret != -1);
        return ret;
#else
        auto it = _event_map.find(fd);
        if (it != _event_map.end()) {
            it->second->event = event;
        }
        cb(it != _event_map.end());
        return it != _event_map.end() ? 0 : -1;
#endif // HAS_EPOLL
    }
    async([this, fd, event, cb]() mutable {
        modifyEvent(fd, event, std::move(cb));
    });
    return 0;
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
    //写数据到管道,唤醒主线程  [AUTO-TRANSLATED:2ead8182]
    //Write data to the pipe and wake up the main thread
    _pipe.write("", 1);
    return ret;
}

bool EventPoller::isCurrentThread() {
    return !_loop_thread || _loop_thread->get_id() == this_thread::get_id();
}

inline void EventPoller::onPipeEvent(bool flush) {
    char buf[1024];
    int err = 0;
    if (!flush) {
       for (;;) {
         if ((err = _pipe.read(buf, sizeof(buf))) > 0) {
             // 读到管道数据,继续读,直到读空为止  [AUTO-TRANSLATED:47bd325c]
             //Read data from the pipe, continue reading until it's empty
             continue;
         }
         if (err == 0 || get_uv_error(true) != UV_EAGAIN) {
             // 收到eof或非EAGAIN(无更多数据)错误,说明管道无效了,重新打开管道  [AUTO-TRANSLATED:5f7a013d]
             //Received eof or non-EAGAIN (no more data) error, indicating that the pipe is invalid, reopen the pipe
             ErrorL << "Invalid pipe fd of event poller, reopen it";
             delEvent(_pipe.readFD());
             _pipe.reOpen();
             addEventPipe();
         }
         break;
      }
    }

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
            ErrorL << "Exception occurred when do async task: " << ex.what();
        }
    });
}

SocketRecvBuffer::Ptr EventPoller::getSharedBuffer(bool is_udp) {
#if !defined(__linux) && !defined(__linux__)
    // 非Linux平台下，tcp和udp共享recvfrom方案，使用同一个buffer  [AUTO-TRANSLATED:2d2ee7bf]
    //On non-Linux platforms, tcp and udp share the recvfrom scheme, using the same buffer
    is_udp = 0;
#endif
    auto ret = _shared_buffer[is_udp].lock();
    if (!ret) {
        ret = SocketRecvBuffer::create(is_udp);
        _shared_buffer[is_udp] = ret;
    }
    return ret;
}

thread::id EventPoller::getThreadId() const {
    return _loop_thread ? _loop_thread->get_id() : thread::id();
}

const std::string& EventPoller::getThreadName() const {
    return _name;
}

static thread_local std::weak_ptr<EventPoller> s_current_poller;

// static
EventPoller::Ptr EventPoller::getCurrentPoller() {
    return s_current_poller.lock();
}

void EventPoller::runLoop(bool blocked, bool ref_self) {
    if (blocked) {
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
            int ret = epoll_wait(_event_fd, events, EPOLL_SIZE, minDelay);
            sleepWakeUp();//用于统计当前线程负载情况
            if (ret <= 0) {
                //超时或被打断  [AUTO-TRANSLATED:7005fded]
                //Timed out or interrupted
                continue;
            }

            _event_cache_expired.clear();

            for (int i = 0; i < ret; ++i) {
                struct epoll_event &ev = events[i];
                int fd = ev.data.fd;
                if (_event_cache_expired.count(fd)) {
                    //event cache refresh
                    continue;
                }

                auto it = _event_map.find(fd);
                if (it == _event_map.end()) {
                    epoll_ctl(_event_fd, EPOLL_CTL_DEL, fd, nullptr);
                    continue;
                }
                auto cb = it->second;
                try {
                    (*cb)(toPoller(ev.events));
                } catch (std::exception &ex) {
                    ErrorL << "Exception occurred when do event task: " << ex.what();
                }
            }
        }
#elif defined(HAS_KQUEUE)
        struct kevent kevents[KEVENT_SIZE];
        while (!_exit_flag) {
            minDelay = getMinDelay();
            struct timespec timeout = { (long)minDelay / 1000, (long)minDelay % 1000 * 1000000 };

            startSleep();
            int ret = kevent(_event_fd, nullptr, 0, kevents, KEVENT_SIZE, minDelay == -1 ? nullptr : &timeout);
            sleepWakeUp();
            if (ret <= 0) {
                continue;
            }

            _event_cache_expired.clear();

            for (int i = 0; i < ret; ++i) {
                auto &kev = kevents[i];
                auto fd = kev.ident;
                if (_event_cache_expired.count(fd)) {
                    // event cache refresh
                    continue;
                }

                auto it = _event_map.find(fd);
                if (it == _event_map.end()) {
                    EV_SET(&kev, fd, kev.filter, EV_DELETE, 0, 0, nullptr);
                    kevent(_event_fd, &kev, 1, nullptr, 0, nullptr);
                    continue;
                }
                auto cb = it->second;
                int event = 0;
                switch (kev.filter) {
                    case EVFILT_READ: event = Event_Read; break;
                    case EVFILT_WRITE: event = Event_Write; break;
                    default: WarnL << "unknown kevent filter: " << kev.filter; break;
                }

                try {
                    (*cb)(event);
                } catch (std::exception &ex) {
                    ErrorL << "Exception occurred when do event task: " << ex.what();
                }
            }
        }
#else
        int ret, max_fd;
        FdSet set_read, set_write, set_err;
        List<Poll_Record::Ptr> callback_list;
        struct timeval tv;
        while (!_exit_flag) {
            //定时器事件中可能操作_event_map  [AUTO-TRANSLATED:f2a50ee2]
            //Possible operations on _event_map in timer events
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
            ret = zl_select(max_fd + 1, &set_read, &set_write, &set_err, minDelay == -1 ? nullptr : &tv);
            sleepWakeUp();//用于统计当前线程负载情况

            if (ret <= 0) {
                //超时或被打断  [AUTO-TRANSLATED:7005fded]
                //Timed out or interrupted
                continue;
            }

            _event_cache_expired.clear();

            //收集select事件类型  [AUTO-TRANSLATED:9a5c41d3]
            //Collect select event types
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

            callback_list.for_each([&](Poll_Record::Ptr &record) {
                if (_event_cache_expired.count(record->fd)) {
                    //event cache refresh
                    return;
                }

                try {
                    record->call_back(record->attach);
                } catch (std::exception &ex) {
                    ErrorL << "Exception occurred when do event task: " << ex.what();
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

int64_t EventPoller::flushDelayTask(uint64_t now_time) {
    decltype(_delay_task_map) task_copy;
    task_copy.swap(_delay_task_map);

    for (auto it = task_copy.begin(); it != task_copy.end() && it->first <= now_time; it = task_copy.erase(it)) {
        //已到期的任务  [AUTO-TRANSLATED:849cdc29]
        //Expired tasks
        try {
            auto next_delay = (*(it->second))();
            if (next_delay) {
                //可重复任务,更新时间截止线  [AUTO-TRANSLATED:c7746a21]
                //Repeatable tasks, update deadline
                _delay_task_map.emplace(next_delay + now_time, std::move(it->second));
            }
        } catch (std::exception &ex) {
            ErrorL << "Exception occurred when do delay task: " << ex.what();
        }
    }

    task_copy.insert(_delay_task_map.begin(), _delay_task_map.end());
    task_copy.swap(_delay_task_map);

    auto it = _delay_task_map.begin();
    if (it == _delay_task_map.end()) {
        //没有剩余的定时器了  [AUTO-TRANSLATED:23b1119e]
        //No remaining timers
        return -1;
    }
    //最近一个定时器的执行延时  [AUTO-TRANSLATED:2535621b]
    //Delay in execution of the last timer
    return it->first - now_time;
}

int64_t EventPoller::getMinDelay() {
    auto it = _delay_task_map.begin();
    if (it == _delay_task_map.end()) {
        //没有剩余的定时器了  [AUTO-TRANSLATED:23b1119e]
        //No remaining timers
        return -1;
    }
    auto now = getCurrentMillisecond();
    if (it->first > now) {
        //所有任务尚未到期  [AUTO-TRANSLATED:8d80eabf]
        //All tasks have not expired
        return it->first - now;
    }
    //执行已到期的任务并刷新休眠延时  [AUTO-TRANSLATED:cd6348b7]
    //Execute expired tasks and refresh sleep delay
    return flushDelayTask(now);
}

EventPoller::DelayTask::Ptr EventPoller::doDelayTask(uint64_t delay_ms, function<uint64_t()> task) {
    DelayTask::Ptr ret = std::make_shared<DelayTask>(std::move(task));
    auto time_line = getCurrentMillisecond() + delay_ms;
    async_first([time_line, ret, this]() {
        //异步执行的目的是刷新select或epoll的休眠时间  [AUTO-TRANSLATED:a6b5c8d7]
        //The purpose of asynchronous execution is to refresh the sleep time of select or epoll
        _delay_task_map.emplace(time_line, ret);
    });
    return ret;
}


///////////////////////////////////////////////

static size_t s_pool_size = 0;
static bool s_enable_cpu_affinity = true;

INSTANCE_IMP(EventPollerPool)

EventPoller::Ptr EventPollerPool::getFirstPoller() {
    return static_pointer_cast<EventPoller>(_threads.front());
}

EventPoller::Ptr EventPollerPool::getPoller(bool prefer_current_thread) {
    auto poller = EventPoller::getCurrentPoller();
    if (prefer_current_thread && _prefer_current_thread && poller) {
        return poller;
    }
    return static_pointer_cast<EventPoller>(getExecutor());
}

void EventPollerPool::preferCurrentThread(bool flag) {
    _prefer_current_thread = flag;
}

const std::string EventPollerPool::kOnStarted = "kBroadcastEventPollerPoolStarted";

EventPollerPool::EventPollerPool() {
    auto size = addPoller("event poller", s_pool_size, ThreadPool::PRIORITY_HIGHEST, true, s_enable_cpu_affinity);
    NOTICE_EMIT(EventPollerPoolOnStartedArgs, kOnStarted, *this, size);
    InfoL << "EventPoller created size: " << size;
}

void EventPollerPool::setPoolSize(size_t size) {
    s_pool_size = size;
}

void EventPollerPool::enableCpuAffinity(bool enable) {
    s_enable_cpu_affinity = enable;
}

}  // namespace toolkit

