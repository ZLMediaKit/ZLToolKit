/*
 * Session.h
 *
 *  Created on: 2015年10月27日
 *      Author: root
 */

#ifndef SERVER_SESSION_H_
#define SERVER_SESSION_H_
#include <memory>
#include "Util/logger.h"
#include "Thread/ThreadPool.hpp"
#include "Network/Socket.hpp"
using namespace std;
using namespace ZL::Thread;
using namespace ZL::Network;
using namespace ZL::Util;

namespace ZL {
namespace Session {

class Session: public std::enable_shared_from_this<Session> {
public:
	Session(const shared_ptr<ThreadPool> &_th, const Socket_ptr &_sock) :
			sock(_sock), th(_th) {
		//DebugL<<EventPoller::Instance().isMainThread();
		localIp = sock->get_local_ip();
		peerIp = sock->get_peer_ip();
	}
	virtual ~Session() {
		//DebugL<<EventPoller::Instance().isMainThread();
		TraceL << endl;
	}
	virtual void onRecv(const string &buf) {
		//DebugL<<EventPoller::Instance().isMainThread();
		this->send(buf);
	}
	virtual void onError(const SockException &err) {
		//DebugL<<EventPoller::Instance().isMainThread();
		TraceL << err.what();
	}
	virtual void onManager() {
		//DebugL<<EventPoller::Instance().isMainThread();
		//TraceL << endl;
	}
	virtual void send(const string &buf) {
		//DebugL<<EventPoller::Instance().isMainThread();
		sock->send(buf);
	}
	virtual void send(const char *buf, int size) {
		//DebugL<<EventPoller::Instance().isMainThread();
		sock->send(buf, size);
	}
	virtual void shutdown(int delay_sec = 0) {
		//DebugL<<EventPoller::Instance().isMainThread();
		if (delay_sec > 0) {
			weak_ptr<Session> weakSelf = shared_from_this();
			AsyncTaskThread::Instance().DoTaskDelay(
					reinterpret_cast<uint64_t>(this), delay_sec * 1000,
					[weakSelf]() {
						shared_ptr<Session> strongSelf=weakSelf.lock();
						if(!strongSelf) {
							return false;
						}
						strongSelf->sock->emitErr(SockException(Err_other, "self shutdown"));
						return false;
					});
		} else {
			sock->emitErr(SockException(Err_other, "self shutdown"));
		}
	}
	void postTask_back(const Task &task, int delay_sec = 0) {
		//DebugL<<EventPoller::Instance().isMainThread();
		if (delay_sec > 0) {
			shared_ptr<ThreadPool> pool = th;
			AsyncTaskThread::Instance().DoTaskDelay(
					reinterpret_cast<uint64_t>(this), delay_sec * 1000,
					[task,pool]() {
						pool->post(task);
						return false;
					});
		} else {
			th->post(task);
		}
	}
	void postTask_front(const Task &task) {
		//DebugL<<EventPoller::Instance().isMainThread();
		th->post_first(task);
	}

	const string& getLocalIp() const {
		return localIp;
	}

	const string& getPeerIp() const {
		return peerIp;
	}

protected:
	Socket_ptr sock;
	shared_ptr<ThreadPool> th;
	string localIp;
	string peerIp;
};
} /* namespace Session */
} /* namespace ZL */

#endif /* SERVER_SESSION_H_ */
