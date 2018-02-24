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
#include <unordered_map>
#include "PipeWrap.h"
#include "Util/logger.h"
#include "Util/util.h"

using namespace std;
using namespace ZL::Util;


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

typedef enum {
	Sig_Exit = 0, //关闭监听
	Sig_Async, //异步
} Sigal_Type;

typedef function<void(int event)> PollEventCB;
typedef function<void(bool success)> PollDelCB;
typedef function<void(void)> PollAsyncCB;
typedef PollAsyncCB PollSyncCB;

#ifndef  HAS_EPOLL
typedef struct {
	Poll_Event event;
	PollEventCB callBack;
	int attach;
	void operator()(int _event) const{
		callBack(_event);
	}
	void operator()() const{
		callBack(attach);
	}
} Poll_Record;
#endif //HAS_EPOLL

class EventPoller {
public:
	EventPoller(bool enableSelfRun = false);
	virtual ~EventPoller();
	static EventPoller &Instance(bool enableSelfRun = false) {
        static EventPoller *instance(new EventPoller(enableSelfRun));
		return *instance;
	}
	static void Destory() {
		delete &EventPoller::Instance();
	}
	int addEvent(int fd, int event, const PollEventCB &eventCb);
	int delEvent(int fd, const PollDelCB &delCb = nullptr);
	int modifyEvent(int fd, int event);

	void async(const PollAsyncCB &asyncCb);
	void sync(const PollSyncCB &syncCb);

	void runLoop();
	void shutdown();
	bool isMainThread();
private:
	void initPoll();
	inline int sigalPipe(uint64_t type, uint64_t i64_size = 0, uint64_t *buf = NULL);
	inline bool handlePipeEvent();
	inline Sigal_Type _handlePipeEvent(uint64_t type, uint64_t i64_size, uint64_t *buf);

	PipeWrap _pipe;
	thread *_loopThread = nullptr;
	thread::id _mainThreadId;
	mutex _mtx_event_map;
#if defined(HAS_EPOLL)
	int _epoll_fd = -1;
	unordered_map<int, PollEventCB> _event_map;
#else
	unordered_map<int, Poll_Record> _event_map;
#endif //HAS_EPOLL
	string _pipeBuffer;
};

#define ASYNC_TRACE(...) {\
							/*TraceL;*/\
							EventPoller::Instance().async(__VA_ARGS__);\
						}
#define SYNC_TRACE(...) {\
							/*TraceL;*/\
							EventPoller::Instance().sync(__VA_ARGS__);\
						}

}  // namespace Poller
}  // namespace ZL
#endif /* EventPoller_h */









