/*
 * TcpServer.h
 *
 *  Created on: 2016年8月9日
 *      Author: xzl
 */

#ifndef TCPSERVER_TCPSERVER_H_
#define TCPSERVER_TCPSERVER_H_

#include "Network/Socket.hpp"
#include "Thread/WorkThreadPool.h"
#include <exception>
#include "Util/util.h"
#include <functional>
#include "Poller/Timer.hpp"
#include <memory>
#include "Util/logger.h"
#include "Thread/semaphore.hpp"
using namespace std;
using namespace ZL::Thread;
using namespace ZL::Util;
using namespace ZL::Poller;

namespace ZL {
namespace Network {

template<typename Session>
class TcpServer {
public:
	TcpServer() :
			socket(new Socket()), threadPool(thread::hardware_concurrency()) {
	}

	~TcpServer() {
		TraceL << "start clean...";
		semaphore sem;
		EventPoller::Instance().sendAsync([&]() {
			timer.reset();
			socket.reset();
			while (sessionMap.size() != 0) {
				sessionMap.begin()->second->shutdown();
			}
			sem.post();
		});
		sem.wait();
		threadPool.wait();
		TraceL << "clean completed!";
	}
	void start(uint16_t port, const std::string& host = "0.0.0.0",
			uint32_t backlog = 1024) {
		bool success = socket->listen(port, host.c_str(), backlog);
		if (!success) {
			throw std::runtime_error(
					StrPrinter << "监听本地端口[" << host << ":" << port << "]失败:"
							<< strerror(errno) << endl);
		}
		socket->setOnAccept(
				bind(&TcpServer::onAcceptConnection, this, placeholders::_1));
		timer.reset(new Timer(2, [this]()->bool {
			this->onManagerSession();
			return true;
		}));
		InfoL << "TCP Server listening on " << host << ":" << port;
	}

private:

	Socket_ptr socket;
	shared_ptr<Timer> timer;
	std::unordered_map<Socket *, shared_ptr<Session> > sessionMap;
	WorkThreadPool threadPool;

	void onAcceptConnection(const Socket_ptr & sock) {
		// DebugL<<EventPoller::Instance().isMainThread();
		// 接收到客户端连接请求
		auto th = threadPool.getWorkThread();
		sessionMap.emplace(static_cast<Socket*>(sock.get()),
				std::make_shared<Session>(th, sock));
		sock->setOnRead(
				bind(&TcpServer::onSocketRecv, this, sock.get(),
						placeholders::_1, placeholders::_2));
		sock->setOnErr(
				bind(&TcpServer::onSocketErr, this, sock.get(),
						placeholders::_1));
	}

	void onSocketRecv(Socket* sender, const char *data, int size) {
		//DebugL<<EventPoller::Instance().isMainThread();
		//收到客户端数据
		auto it = sessionMap.find(sender);
		if (it == sessionMap.end()) {
			sender->setOnRead(nullptr);
			WarnL<<"未处理的套接字事件";
			return;
		}
		string buf(data, size);
		weak_ptr<Session> weakSession = it->second;
		it->second->postTask_back([weakSession,buf]() {
			auto strongSession=weakSession.lock();
			if(!strongSession) {
				return;
			}
			strongSession->onRecv(buf);
		});
	}
	void onSocketErr(Socket *sender, const SockException &err) {
		//DebugL<<EventPoller::Instance().isMainThread();
		auto it = sessionMap.find(sender);
		if (it == sessionMap.end()) {
			sender->setOnErr(nullptr);
			WarnL<<"未处理的套接字事件";
			return;
		}
		auto session = it->second;
		it->second->postTask_front([session,err]() {
			session->onError(err);
		});
		sessionMap.erase(it);
	}
	void onManagerSession() {
		//DebugL<<EventPoller::Instance().isMainThread();
		for (auto &pr : sessionMap) {
			weak_ptr<Session> weakSession = pr.second;
			pr.second->postTask_back([weakSession]() {
				auto strongSession=weakSession.lock();
				if(!strongSession) {
					return;
				}
				strongSession->onManager();
			});
		}
	}
};

} /* namespace Network */
} /* namespace ZL */

#endif /* TCPSERVER_TCPSERVER_H_ */
