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

#include <fcntl.h>
#include <string.h>
#include <list>
#include "SelectWrap.h"
#include "EventPoller.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/uv_errno.h"
#include "Util/TimeTicker.h"
#include "Util/onceToken.h"
#include "Thread/ThreadPool.h"
#include "Network/sockutil.h"


#if defined(HAS_EPOLL)
    #include <sys/epoll.h>
    #define EPOLL_SIZE 1024
    #define toEpoll(event)    (((event) & Event_Read) ? EPOLLIN : 0) \
                            | (((event) & Event_Write) ? EPOLLOUT : 0) \
                            | (((event) & Event_Error) ? (EPOLLHUP | EPOLLERR) : 0) \
                            | (((event) & Event_LT) ?  0 : EPOLLET)
    #define toPoller(epoll_event) (((epoll_event) & EPOLLIN) ? Event_Read : 0) \
                                | (((epoll_event) & EPOLLOUT) ? Event_Write : 0) \
                                | (((epoll_event) & EPOLLHUP) ? Event_Error : 0) \
                                | (((epoll_event) & EPOLLERR) ? Event_Error : 0)
#endif //HAS_EPOLL

namespace toolkit {

EventPoller &EventPoller::Instance() {
    return *(EventPollerPool::Instance().getFirstPoller());
}
void EventPoller::Destory() {
    Instance().shutdown();
    EventPollerPool::Destory();
}

EventPoller::EventPoller() {
    SockUtil::setNoBlocked(_pipe.readFD());
    SockUtil::setNoBlocked(_pipe.writeFD());

#if defined(HAS_EPOLL)
    _epoll_fd = epoll_create(EPOLL_SIZE);
    if (_epoll_fd == -1) {
        throw runtime_error(StrPrinter << "创建epoll文件描述符失败:" << get_uv_errmsg());
    }

    if (addEvent(_pipe.readFD(), Event_Read | Event_Error, [](int event) {}) == -1) {
        throw std::runtime_error("epoll添加管道失败");
    }
#endif //HAS_EPOLL

    _mainThreadId = this_thread::get_id();
}


void EventPoller::shutdown() {
    async_first([](){
        throw ExitException();
    });
    if (_loopThread) {
        try {
            _loopThread->join();
        }catch (std::exception &ex){
            ErrorL << "catch exception:" << ex.what();
        }
        delete _loopThread;
        _loopThread = nullptr;
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
    _mainThreadId = this_thread::get_id();
    onPipeEvent();
    InfoL << this;
}

int EventPoller::addEvent(int fd, int event, const PollEventCB &cb) {
    TimeTicker();
    if (!cb) {
        WarnL << "PollEventCB 为空!";
        return -1;
    }
#if defined(HAS_EPOLL)
    lock_guard<mutex> lck(_mtx_event_map);
    struct epoll_event ev = {0};
    ev.events = toEpoll(event);
    ev.data.fd = fd;
    int ret = epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    if (ret == 0) {
        _event_map.emplace(fd, std::make_shared<PollEventCB>(cb));
    }
    return ret;
#else
    if (isMainThread()) {
        lock_guard<mutex> lck(_mtx_event_map);
        Poll_Record::Ptr record(new Poll_Record);
        record->event = event;
        record->callBack = cb;
        _event_map.emplace(fd, record);
        return 0;
    }
    async([this, fd, event, cb]() {
        addEvent(fd, event, cb);
    });
    return 0;
#endif //HAS_EPOLL
}

int EventPoller::delEvent(int fd, const PollDelCB &delCb) {
    TimeTicker();
    if (!delCb) {
        const_cast<PollDelCB &>(delCb) = [](bool success) {};
    }
#if defined(HAS_EPOLL)
    int ret0 = epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    int ret1 = 0;
    {
        lock_guard<mutex> lck(_mtx_event_map);
        ret1 = _event_map.erase(fd);
    }
    bool success = ret0 == 0 && ret1 > 0;
    delCb(success);
    return success ? 0 : -1;
#else
    if (isMainThread()) {
        lock_guard<mutex> lck(_mtx_event_map);
        if (_event_map.erase(fd)) {
            delCb(true);
        }else {
            delCb(false);
        }
        return 0;
    }
    async([this, fd, delCb]() {
        delEvent(fd, delCb);
    });
    return 0;
#endif //HAS_EPOLL
}

int EventPoller::modifyEvent(int fd, int event) {
    TimeTicker();
    //TraceL<<fd<<" "<<event;
#if defined(HAS_EPOLL)
    struct epoll_event ev = {0};
    ev.events = toEpoll(event);
    ev.data.fd = fd;
    return epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &ev);
#else
    if (isMainThread()) {
        lock_guard<mutex> lck(_mtx_event_map);
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

bool EventPoller::sync(const TaskExecutor::Task &task) {
    return sync_l(task, false);
}

bool EventPoller::sync_first(const TaskExecutor::Task &task) {
    return sync_l(task, true);
}
bool EventPoller::sync_l(const TaskExecutor::Task &task,bool first){
    TimeTicker();
    if (!task) {
        return false;
    }
    semaphore sem;
    async_l([&]() {
        task();
        sem.post();
    }, true,first);
    sem.wait();
    return true;
}

bool EventPoller::async(const TaskExecutor::Task &task, bool may_sync) {
    return async_l(task,may_sync, false);
}
bool EventPoller::async_first(const TaskExecutor::Task &task, bool may_sync) {
    return async_l(task,may_sync, true);
}

bool EventPoller::async_l(const TaskExecutor::Task &task,bool may_sync, bool first) {
    TimeTicker();
    if (!task) {
        return false;
    }
    if (may_sync && _mainThreadId == this_thread::get_id()) {
        task();
        return true;
    }

    {
        lock_guard<mutex> lck(_mtx_task);
        if(first){
            _list_task.emplace_front(task);
        }else{
            _list_task.emplace_back(task);
        }
    }
    //写数据到管道,唤醒主线程
    _pipe.write("",1);
    return true;
}

bool EventPoller::isMainThread() {
    return _mainThreadId == this_thread::get_id();
}

inline bool EventPoller::onPipeEvent() {
    TimeTicker();
    char buf[1024];
    int err = 0;
    do {
        if (_pipe.read(buf, sizeof(buf)) > 0) {
            continue;
        }
        err = get_uv_error(true);
    } while (err != UV_EAGAIN);

    decltype(_list_task) listCopy;
    {
        lock_guard<mutex> lck(_mtx_task);
        listCopy.swap(_list_task);
    }

    bool catchExitException = false;
    listCopy.for_each([&](const TaskExecutor::Task &task){
        try {
            task();
        }catch (ExitException &ex){
            catchExitException = true;
        }catch (std::exception &ex){
            ErrorL << "catch exception:" << ex.what();
        }
    });
    return !catchExitException;
}

void EventPoller::wait() {
    lock_guard<mutex> lck(_mtx_runing);
}
void EventPoller::runLoop(bool blocked) {
    if (blocked) {
        onceToken token([this](){
            //获取锁
            _mtx_runing.lock();
        },[this](){
            //释放锁并标记已经执行过runLoop
            _loopRunned = true;
            _mtx_runing.unlock();
        });

        if(_loopRunned){
            //runLoop已经被执行过了，不能执行两次
            return;
        }
        _mainThreadId = this_thread::get_id();
        _sem_run_started.post();
        ThreadPool::setPriority(ThreadPool::PRIORITY_HIGHEST);
#if defined(HAS_EPOLL)
        struct epoll_event events[EPOLL_SIZE];
        while (true) {
            startSleep();
            int nfds = epoll_wait(_epoll_fd, events, EPOLL_SIZE, -1);
            sleepWakeUp();

            TimeTicker();
            if (nfds == -1) {
                WarnL << "epoll_wait() interrupted:" << get_uv_errmsg();
                continue;
            }

            for (int i = 0; i < nfds; ++i) {
                struct epoll_event &ev = events[i];
                int fd = ev.data.fd;
                int event = toPoller(ev.events);
                if (fd == _pipe.readFD()) {
                    //inner pipe event
                    if (event & Event_Error) {
                        WarnL << "Poller 异常退出监听。";
                        return;
                    }
                    if (!onPipeEvent()) {
                        return;
                    }
                    continue;
                }
                // other event
                std::shared_ptr<PollEventCB> eventCb;
                {
                    lock_guard<mutex> lck(_mtx_event_map);
                    auto it = _event_map.find(fd);
                    if (it == _event_map.end()) {
                        WarnL << "未找到Poll事件回调对象!";
                        epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                        continue;
                    }
                    eventCb = it->second;
                }
                try{
                    (*eventCb)(event);
                }catch (std::exception &ex){
                    ErrorL << "catch exception:" << ex.what();
                }
            }
        }
#else
        int ret, maxFd;
        FdSet Set_read, Set_write, Set_err;
        List<Poll_Record::Ptr> listCB;
        while (true) {
            Set_read.fdZero();
            Set_write.fdZero();
            Set_err.fdZero();
            Set_read.fdSet(_pipe.readFD()); //监听管道可读事件
            maxFd = _pipe.readFD();
            {
                lock_guard<mutex> lck(_mtx_event_map);
                for (auto &pr : _event_map) {
                    if (pr.first > maxFd) {
                        maxFd = pr.first;
                    }
                    if (pr.second->event & Event_Read) {
                        Set_read.fdSet(pr.first);//监听管道可读事件
                    }
                    if (pr.second->event & Event_Write) {
                        Set_write.fdSet(pr.first);//监听管道可写事件
                    }
                    if (pr.second->event & Event_Error) {
                        Set_err.fdSet(pr.first);//监听管道错误事件
                    }
                }
            }
            startSleep();
            ret = zl_select(maxFd + 1, &Set_read, &Set_write, &Set_err, NULL);
            sleepWakeUp();
            if (ret < 1) {
                WarnL << "select() interrupted:" << get_uv_errmsg();
                continue;
            }

            if (Set_read.isSet(_pipe.readFD())) {
                //判断有否监听操作
                if (!onPipeEvent()) {
                    break;
                }
                if (ret == 1) {
                    //没有其他事件
                    continue;
                }
            }

            {
                lock_guard<mutex> lck(_mtx_event_map);
                for (auto &pr : _event_map) {
                    int event = 0;
                    if (Set_read.isSet(pr.first)) {
                        event |= Event_Read;
                    }
                    if (Set_write.isSet(pr.first)) {
                        event |= Event_Write;
                    }
                    if (Set_err.isSet(pr.first)) {
                        event |= Event_Error;
                    }
                    if (event != 0) {
                        pr.second->attach = event;
                        listCB.emplace_back(pr.second);
                    }
                }
            }
            listCB.for_each([](Poll_Record::Ptr &record){
                try{
                    record->callBack(record->attach);
                }catch (std::exception &ex){
                    ErrorL << "catch exception:" << ex.what();
                }
            });
            listCB.clear();
        }
#endif //HAS_EPOLL
    }else{
        _loopThread = new thread(&EventPoller::runLoop, this, true);
        _sem_run_started.wait();
    }
}


///////////////////////////////////////////////
static EventPollerPool::Ptr s_pool_instance;
EventPollerPool &EventPollerPool::Instance() {
    static onceToken s_token([](){
        s_pool_instance.reset(new EventPollerPool);
    });
    return *s_pool_instance;
}
void EventPollerPool::Destory(){
    s_pool_instance.reset();
}

EventPoller::Ptr EventPollerPool::getFirstPoller(){
    return dynamic_pointer_cast<EventPoller>(_threads.front());
}

EventPoller::Ptr EventPollerPool::getPoller(){
    return dynamic_pointer_cast<EventPoller>(getExecutor());
}

EventPollerPool::EventPollerPool(): TaskExecutorGetterImp([](){
    auto ret = std::make_shared<EventPoller>();
    ret->runLoop(false);
    return ret;
}){}


}  // namespace toolkit

