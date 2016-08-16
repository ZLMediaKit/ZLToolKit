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
		localIp = sock->get_local_ip();
		peerIp = sock->get_peer_ip();
	}
	virtual ~Session() {
		TraceL << endl;
	}
	virtual void onRecv(const string &buf) =0;
	virtual void onError(const SockException &err) =0;
	virtual void onManager() =0;
	void postTask_back(const Task &task) {
		th->post(task);
	}
	void postTask_front(const Task &task) {
		th->post_first(task);
	}
	const string& getLocalIp() const {
		return localIp;
	}
	const string& getPeerIp() const {
		return peerIp;
	}
protected:
	virtual void shutdown() {
		sock->emitErr(SockException(Err_other, "self shutdown"));
	}
	virtual void send(const string &buf) {
		sock->send(buf);
	}
	virtual void send(const char *buf, int size) {
		sock->send(buf, size);
	}
	Socket_ptr sock;
	shared_ptr<ThreadPool> th;
	string localIp;
	string peerIp;
};
} /* namespace Session */
} /* namespace ZL */

#endif /* SERVER_SESSION_H_ */
