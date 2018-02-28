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
#include "Thread/ThreadPool.h"
#include "Network/sockutil.h"

using namespace ZL::Util;
using namespace ZL::Thread;
using namespace ZL::Network;

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

namespace ZL {
namespace Poller {

EventPoller::EventPoller(bool enableSelfRun) {
#if defined(HAS_EPOLL)
    _epoll_fd = epoll_create(EPOLL_SIZE);
    if (_epoll_fd == -1) {
        throw runtime_error(StrPrinter << "创建epoll文件描述符失败:" << get_uv_errmsg());
    }
#endif //HAS_EPOLL
    initPoll();
    if (enableSelfRun) {
        _loopThread = new thread(&EventPoller::runLoop, this);
        _mainThreadId = _loopThread->get_id();
    } else {
        _mainThreadId = this_thread::get_id();
    }
}

inline int EventPoller::sigalPipe(uint64_t type, uint64_t i64_size, uint64_t *buf) {
    uint64_t *pipeBuf = new uint64_t[2 + i64_size];
    pipeBuf[0] = type;
    pipeBuf[1] = i64_size;
    if (i64_size) {
        memcpy(pipeBuf + 2, buf, i64_size * sizeof(uint64_t));
    }
    auto ret = _pipe.write(pipeBuf, (2 + i64_size) * sizeof(uint64_t));
    delete[] pipeBuf;
    return ret;
}

void EventPoller::shutdown() {
    sigalPipe(Sig_Exit);
}

EventPoller::~EventPoller() {
    shutdown();

    if (_loopThread) {
        _loopThread->join();
        delete _loopThread;
        _loopThread = nullptr;
    }

#if defined(HAS_EPOLL)
    if (_epoll_fd != -1) {
        close(_epoll_fd);
        _epoll_fd = -1;
    }
#endif //defined(HAS_EPOLL)
    //退出前清理管道中的数据
    _mainThreadId = this_thread::get_id();
    handlePipeEvent();
    InfoL;
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
        _event_map.emplace(fd, cb);
    }
    return ret;
#else
    if (isMainThread()) {
        lock_guard<mutex> lck(_mtx_event_map);
        Poll_Record record;
        record.event = (Poll_Event)event;
        record.callBack = cb;
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
            it->second.event = (Poll_Event)event;
        }
        return 0;
    }
    async([this, fd, event]() {
        modifyEvent(fd, event);
    });
    return 0;
#endif //HAS_EPOLL
}

void EventPoller::sync(const PollAsyncCB &syncCb) {
    TimeTicker();
    if (!syncCb) {
        return;
    }
    semaphore sem;
    async([&]() {
        syncCb();
        sem.post();
    });
    sem.wait();
}

void EventPoller::async(const PollAsyncCB &asyncCb) {
    TimeTicker();
    if (!asyncCb) {
        return;
    }
    if (_mainThreadId == this_thread::get_id()) {
        asyncCb();
        return;
    }
    std::shared_ptr<Ticker> pTicker(new Ticker(5, "wake up main thread", WarnL, true));
    auto lam = [asyncCb, pTicker]() {
        const_cast<std::shared_ptr<Ticker> &>(pTicker).reset();
        asyncCb();
    };
    uint64_t buf[1] = {(uint64_t) (new PollAsyncCB(lam))};
    sigalPipe(Sig_Async, 1, buf);
}

bool EventPoller::isMainThread() {
    return _mainThreadId == this_thread::get_id();
}

inline Sigal_Type EventPoller::_handlePipeEvent(uint64_t type, uint64_t i64_size, uint64_t *buf) {
    switch (type) {
        case Sig_Async: {
            PollAsyncCB **cb = (PollAsyncCB **) buf;
            try {
                (*cb)->operator()();
            }
            catch (std::exception &ex) {
                FatalL << ex.what();
            }
            delete *cb;
        }
            break;
        default:
            break;
    }
    return (Sigal_Type) type;
}

inline bool EventPoller::handlePipeEvent() {
    TimeTicker();
    int nread;
    char buf[1024];
    int err = 0;
    do {
        nread = _pipe.read(buf, sizeof(buf));
        if (nread > 0) {
            _pipeBuffer.append(buf, nread);
            continue;
        }
        err = get_uv_error(true);
    } while (err != UV_EAGAIN);

    bool ret = true;
    while (_pipeBuffer.size() >= 2 * sizeof(uint64_t)) {
        uint64_t type = *((uint64_t *) _pipeBuffer.data());
        uint64_t slinceSize = *((uint64_t *) (_pipeBuffer.data()) + 1);
        uint64_t slinceByte = (2 + slinceSize) * sizeof(uint64_t);
        if (slinceByte > _pipeBuffer.size()) {
            break;
        }
        uint64_t *ptr = (uint64_t *) (_pipeBuffer.data()) + 2;
        if (Sig_Exit == _handlePipeEvent(type, slinceSize, ptr)) {
            ret = false;
        }
        _pipeBuffer.erase(0, slinceByte);
    }
    return ret;
}

void EventPoller::initPoll() {
#if defined(HAS_EPOLL)
    if (addEvent(_pipe.readFD(), Event_Read | Event_Error, [](int event) {}) == -1) {
        FatalL << "epoll添加管道失败" << endl;
        std::runtime_error("epoll添加管道失败");
    }
#endif //HAS_EPOLL
}

void EventPoller::runLoop() {
    _mainThreadId = this_thread::get_id();
    ThreadPool::setPriority(ThreadPool::PRIORITY_HIGHEST);
#if defined(HAS_EPOLL)
    struct epoll_event events[EPOLL_SIZE];
    while (true) {
        int nfds = epoll_wait(_epoll_fd, events, EPOLL_SIZE, -1);
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
                if (!handlePipeEvent()) {
                    InfoL << "Poller 退出监听。";
                    return;
                }
                continue;
            }
            // other event
            PollEventCB eventCb;
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
            eventCb(event);
        }
    }
#else
    int ret, maxFd;
    FdSet Set_read, Set_write, Set_err;
    list<unordered_map<int, Poll_Record>::value_type> listCB;
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
                if (pr.second.event & Event_Read) {
                    Set_read.fdSet(pr.first);//监听管道可读事件
                }
                if (pr.second.event & Event_Write) {
                    Set_write.fdSet(pr.first);//监听管道可写事件
                }
                if (pr.second.event & Event_Error) {
                    Set_err.fdSet(pr.first);//监听管道错误事件
                }
            }
        }
        ret = zl_select(maxFd + 1, &Set_read, &Set_write, &Set_err, NULL);
        if (ret < 1) {
            WarnL << "select() interrupted:" << get_uv_errmsg();
            continue;
        }

        if (Set_read.isSet(_pipe.readFD())) {
            //判断有否监听操作
            if (!handlePipeEvent()) {
                InfoL << "EventPoller exiting...";
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
                    pr.second.attach = event;
                    listCB.push_back(pr);
                }
            }
        }
        for (auto &pr : listCB) {
            pr.second();
        }
        listCB.clear();
    }
#endif //HAS_EPOLL
}

}  // namespace Poller
}  // namespace ZL

