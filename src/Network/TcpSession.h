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
using namespace ZL::Util;

namespace ZL {
namespace Network {

template<int MaxCount>
class TcpSession: public std::enable_shared_from_this<TcpSession<MaxCount> > {
public:
	TcpSession(const std::shared_ptr<ThreadPool> &_th, const Socket::Ptr &_sock) :
			sock(_sock), th(_th) {
		localIp = sock->get_local_ip();
		peerIp = sock->get_peer_ip();
		localPort = sock->get_local_port();
		peerPort = sock->get_peer_port();

		lock_guard<recursive_mutex> lck(stackMutex());
		static uint64_t maxSeq(0);
		sessionSeq = maxSeq++;
		auto &stack = getStack();
		stack.emplace(this);

		if(stack.size() > MaxCount){
			auto it = stack.begin();
			(*it)->safeShutdown();
			stack.erase(it);
			WarnL << "超过TCP个数限制:" << MaxCount;
		}
	}
	virtual ~TcpSession() {
		lock_guard<recursive_mutex> lck(stackMutex());
		getStack().erase(this);
	}
	virtual void onRecv(const Socket::Buffer::Ptr &) =0;
	virtual void onError(const SockException &err) =0;
	virtual void onManager() =0;
	void async(const Task &task) {
		th->async(task);
	}
	void async_first(const Task &task) {
		th->async_first(task);
	}

protected:
	const string& getLocalIp() const {
		return localIp;
	}
	const string& getPeerIp() const {
		return peerIp;
	}
	uint16_t getLocalPort() const {
		return localPort;
	}
	uint16_t getPeerPort() const {
		return peerPort;
	}
	virtual void shutdown() {
		sock->emitErr(SockException(Err_other, "self shutdown"));
	}
	void safeShutdown(){
		std::weak_ptr<TcpSession> weakSelf = TcpSession<MaxCount>::shared_from_this();
		async_first([weakSelf](){
			auto strongSelf = weakSelf.lock();
			if(strongSelf){
				strongSelf->shutdown();
			}
		});
	}
	virtual int send(const string &buf) {
		return sock->send(buf);
	}
	virtual int send(const char *buf, int size) {
		return sock->send(buf, size);
	}
	Socket::Ptr sock;
	std::shared_ptr<ThreadPool> th;
	string localIp;
	string peerIp;
	uint16_t localPort;
	uint16_t peerPort;

	uint64_t sessionSeq; //会话栈顺序
	struct Comparer {
		bool operator()(TcpSession *x, TcpSession *y) const {
			return x->sessionSeq < y->sessionSeq;
		}
	};
	static recursive_mutex &stackMutex(){
		static recursive_mutex mtx;
		return mtx;
	}
	//RTSP会话栈,先创建的在前面
	static set<TcpSession *, Comparer> &getStack(){
		static set<TcpSession *, Comparer> stack;
		return stack;
	}
};


} /* namespace Session */
} /* namespace ZL */

#endif /* SERVER_SESSION_H_ */
