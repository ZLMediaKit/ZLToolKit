//
//  Poller.hpp
//  G平台
//
//  Created by boyo on 16/4/12.
//  Copyright © 2016年 boyo. All rights reserved.
//

#ifndef Poller_hpp
#define Poller_hpp

#include <stdio.h>
#include <functional>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <string>
#include <mutex>
using namespace std;

#ifdef __linux__
#define HAS_EPOLL
#endif //__linux__
namespace ZL {
namespace Poller {

typedef enum {
	Event_Read = 1 << 0, //读事件
	Event_Write = 1 << 1, //写事件
	Event_Error = 1 << 2, //错误事件
} Poll_Event;

typedef enum {
	Sig_Exit = 0, //关闭监听
	Sig_Async, //异步
} Sigal_Type;

typedef function<void(int event)> PollEventCB;
typedef function<void(bool success)> PollDelCB;
typedef function<void(void)> PollAsyncCB;

#ifndef  HAS_EPOLL
typedef struct {
	Poll_Event event;
	PollEventCB callBack;
	int attach;
} Poll_Record;
#endif //HAS_EPOLL

class EventPoller {
public:
	EventPoller(bool enableSelfRun=false);
	virtual ~EventPoller();
	static EventPoller &Instance() {
		static EventPoller *instance(new EventPoller());
		return *instance;
	}

	int addEvent(int fd, int event, PollEventCB &&eventCb);
	int delEvent(int fd, PollDelCB &&delCb = nullptr);
	int modifyEvent(int fd, int event);
	void sendAsync(PollAsyncCB &&asyncCb);
	void runLoop();
	void shutdown();
	bool isMainThread();
private:
	void initPoll();

	inline int sigalPipe(uint64_t type, uint64_t i64_size = 0, uint64_t *buf =
	NULL);
	inline bool handlePipeEvent();
	inline Sigal_Type _handlePipeEvent(const uint64_t *ptr);

	int pipe_fd[2] = { -1, -1 };
	thread *loopThread = nullptr;
	thread::id mainThreadId;
#ifdef HAS_EPOLL
	mutex mtx_event_map;
	int epoll_fd = -1;
	unordered_map<int,PollEventCB> event_map;
#else
	unordered_map<int, Poll_Record> event_map;
#endif //HAS_EPOLL
};

}  // namespace Poller
}  // namespace ZL
#endif /* Poller_hpp */
