//
//  Poller.cpp
//  G平台
//
//  Created by boyo on 16/4/12.
//  Copyright © 2016年 boyo. All rights reserved.
//
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <list>
#include "Util/util.h"
#include "Util/logger.h"
#include "EventPoller.hpp"
#include "Network/sockutil.h"
#ifdef HAS_EPOLL
#include <sys/epoll.h>

#define toEpoll(event)	(((event) & Event_Read) ? EPOLLIN : 0) \
						| (((event) & Event_Write) ? EPOLLOUT : 0) \
						| (((event) & Event_Error) ? (EPOLLHUP | EPOLLERR) : 0) | EPOLLET

#define toPoller(epoll_event) (((epoll_event) & EPOLLIN) ? Event_Read : 0) \
							| (((epoll_event) & EPOLLOUT) ? Event_Write : 0) \
							| (((epoll_event) & EPOLLHUP) ? Event_Error : 0) \
							| (((epoll_event) & EPOLLERR) ? Event_Error : 0)
#endif //HAS_EPOLL
using namespace ZL::Util;
using namespace ZL::Network;
namespace ZL {
namespace Poller {

EventPoller::EventPoller(bool enableSelfRun) {
	if (pipe(pipe_fd)) {
		throw runtime_error(StrPrinter << "创建管道失败：" << errno << endl);
	}
	SockUtil::setNoBlocked(pipe_fd[0]);
	SockUtil::setNoBlocked(pipe_fd[1]);
#ifdef HAS_EPOLL
	epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	if (epoll_fd == -1) {
		close(pipe_fd[0]);
		close(pipe_fd[1]);
		throw runtime_error(StrPrinter << "创建epoll文件描述符失败：" << errno << endl);
	}
#endif //HAS_EPOLL
	initPoll();
	if (enableSelfRun) {
		loopThread = new thread(&EventPoller::runLoop, this);
		mainThreadId = loopThread->get_id();
	}
}
inline int EventPoller::sigalPipe(uint64_t type, uint64_t i64_size,
		uint64_t *buf) {
	uint64_t new_buf[4] = { type, (uint64_t) i64_size };
	if (i64_size) {
		memcpy(&new_buf[2], buf, i64_size * sizeof(uint64_t));
	}
	return (int) write(pipe_fd[1], new_buf, (2 + i64_size) * sizeof(uint64_t));
}
void EventPoller::shutdown() {
	sigalPipe(Sig_Exit);
	if (loopThread) {
		loopThread->join();
		delete loopThread;
	}
#ifdef HAS_EPOLL
	if (epoll_fd != -1) {
		close(epoll_fd);
		epoll_fd = -1;
	}
#endif //HAS_EPOLL
	if (pipe_fd[0]) {
		close(pipe_fd[0]);
		pipe_fd[0] = -1;
	}
	if (pipe_fd[1]) {
		close(pipe_fd[1]);
		pipe_fd[1] = -1;
	}

}
EventPoller::~EventPoller() {
	shutdown();
	InfoL << endl;
}

int EventPoller::addEvent(int fd, int event, PollEventCB &&cb) {
	if (!cb) {
		WarnL << "PollEventCB 为空!";
		return -1;
	}
#ifdef HAS_EPOLL
	lock_guard<mutex> lck(mtx_event_map);
	struct epoll_event ev = { 0 };
	ev.events = toEpoll(event);
	ev.data.fd = fd;
	int ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
	if (ret == 0) {
		event_map.emplace(fd, cb);
	}
	return ret;
#else
	if (mainThreadId == this_thread::get_id()) {
		Poll_Record record;
		record.event = (Poll_Event) event;
		record.callBack = cb;
		event_map.emplace(fd, record);
		return 0;
	}
	sendAsync([this,fd,event,cb]() {
				this->addEvent(fd,event,const_cast<PollEventCB &&>(cb));
			});
	return 0;
#endif //HAS_EPOLL
}

int EventPoller::delEvent(int fd, PollDelCB &&delCb) {
	if (!delCb) {
		delCb = [](bool success) {};
	}
#ifdef HAS_EPOLL
	int ret0 = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
	int ret1 = 0;
	{
		lock_guard<mutex> lck(mtx_event_map);
		ret1 = event_map.erase(fd);
	}
	bool success = ret0 == 0 && ret1 > 0;
	delCb(success);
	return success;
#else
	if (mainThreadId == this_thread::get_id()) {
		if (event_map.erase(fd)) {
			delCb(true);
		} else {
			delCb(false);
		}
		return 0;
	}
	sendAsync([this,fd,delCb]() {
				this->delEvent(fd, const_cast<PollDelCB &&>(delCb));
			});
	return 0;
#endif //HAS_EPOLL
}
int EventPoller::modifyEvent(int fd, int event) {
#ifdef HAS_EPOLL
	struct epoll_event ev = { 0 };
	ev.events = toEpoll(event);
	ev.data.fd = fd;
	return epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
#else
	if (mainThreadId == this_thread::get_id()) {
		auto it = event_map.find(fd);
		if (it != event_map.end()) {
			it->second.event = (Poll_Event) event;
		}
		return 0;
	}
	sendAsync([this,fd,event]() {
				this->modifyEvent(fd,event);
			});
	return 0;
#endif //HAS_EPOLL
}
void EventPoller::sendAsync(PollAsyncCB &&asyncCb) {
	if (!asyncCb) {
		return;
	}
	if (mainThreadId == this_thread::get_id()) {
		asyncCb();
		return;
	}
	uint64_t buf[1] = { (uint64_t) (new PollAsyncCB(asyncCb)) };
	sigalPipe(Sig_Async, 1, buf);
}

bool EventPoller::isMainThread() {
	return mainThreadId == this_thread::get_id();
}

inline Sigal_Type EventPoller::_handlePipeEvent(const uint64_t *ptr) {
	Sigal_Type type = (Sigal_Type) ptr[0];
	switch (type) {
	case Sig_Async: {
		PollAsyncCB **cb = (PollAsyncCB **) &ptr[2];
		(*cb)->operator()();
		delete *cb;
	}
		break;
	default:
		break;
	}
	return type;
}
inline bool EventPoller::handlePipeEvent() {
	uint64_t buf[256];
	uint64_t nread = (uint64_t) read(pipe_fd[0], buf, sizeof(buf));
	nread /= sizeof(uint64_t);
	if (nread < 2) {
		WarnL << "Poller异常退出！";
		return false;
	}
	uint64_t pos = 0;
	uint64_t slinceSize;
	bool ret = true;
	while (nread) {
		if (_handlePipeEvent(&buf[pos]) == Sig_Exit) {
			ret = false;
		}
		slinceSize = 2 + buf[pos + 1];
		pos += slinceSize;
		nread -= slinceSize;
	}
	return ret;
}
void EventPoller::initPoll() {
#ifdef HAS_EPOLL
	if (addEvent(pipe_fd[0], Event_Read | Event_Error, [](int event) {})
			== -1) {
		FatalL << "epoll添加管道失败" << endl;
		std::runtime_error("epoll添加管道失败");
	}
#else
#endif //HAS_EPOLL
}
void EventPoller::runLoop() {
	mainThreadId = this_thread::get_id();
#ifdef HAS_EPOLL
	struct epoll_event events[1024];
	int nfds = 0;
	while (true) {
		nfds = epoll_wait(epoll_fd, events, 1024, -1);
		if (nfds == -1) {
			if (pipe_fd[0] == -1 || pipe_fd[1] == -1) {
				break;
			}
			WarnL << "epoll_wait() interrupted!";
			continue;
		}

		for (int i = 0; i < nfds; ++i) {
			struct epoll_event &ev = events[i];
			int fd = ev.data.fd;
			int event = toPoller(ev.events);
			if (fd == pipe_fd[0]) {
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
				lock_guard<mutex> lck(mtx_event_map);
				auto it = event_map.find(fd);
				if (it == event_map.end()) {
					WarnL << "未找到Poll事件回调对象!";
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
					continue;
				}
				eventCb = it->second;
			}
			eventCb(event);
		}
	}

#else
	int ret, maxFd;
	fd_set Set_read, Set_write, Set_err;
	list<unordered_map<int, Poll_Record>::value_type> listCB;
	while (true) {
		FD_ZERO(&Set_read);
		FD_ZERO(&Set_write);
		FD_ZERO(&Set_err);

		FD_SET(pipe_fd[0], &Set_read); //监听管道可读事件
		maxFd = pipe_fd[0];

		for (auto &pr : event_map) {
			if (pr.first > maxFd) {
				maxFd = pr.first;
			}
			if (pr.second.event & Event_Read) {
				FD_SET(pr.first, &Set_read); //监听管道可读事件
			}
			if (pr.second.event & Event_Write) {
				FD_SET(pr.first, &Set_write); //监听管道可写事件
			}
			if (pr.second.event & Event_Error) {
				FD_SET(pr.first, &Set_err); //监听管道错误事件
			}
		}
		ret = select(maxFd + 1, &Set_read, &Set_write, &Set_err, NULL);
		if (ret < 1) {
			if (pipe_fd[0] == -1 || pipe_fd[1] == -1) {
				break;
			}
			WarnL << "select() interrupted!";
			continue;
		}

		if (FD_ISSET(pipe_fd[0], &Set_read)) {
			//判断有否监听操作
			if (!handlePipeEvent()) {
				InfoL << "Poller 退出监听。";
				break;
			}
			if (ret == 1) {
				//没有其他事件
				continue;
			}
		}

		for (auto &pr : event_map) {
			int event = 0;
			if (FD_ISSET(pr.first, &Set_read)) {
				event |= Event_Read;
			}
			if (FD_ISSET(pr.first, &Set_write)) {
				event |= Event_Write;
			}
			if (FD_ISSET(pr.first, &Set_err)) {
				event |= Event_Error;
			}
			if (event != 0) {
				pr.second.attach = event;
				listCB.push_back(pr);
			}
		}
		for (auto &pr : listCB) {
			pr.second.callBack(pr.second.attach);
		}
		listCB.clear();
	}
#endif //HAS_EPOLL
}

}  // namespace Poller
}  // namespace ZL

