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

#ifndef TCPSERVER_TCPSERVER_H_
#define TCPSERVER_TCPSERVER_H_

#include <memory>
#include <exception>
#include <functional>
#include "Socket.h"
#include "Util/mini.h"
#include "Util/util.h"
#include "Util/uv_errno.h"
#include "Util/logger.h"
#include "Poller/Timer.h"
#include "Thread/semaphore.h"
#include "Thread/WorkThreadPool.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Poller;
using namespace ZL::Thread;

namespace ZL {
namespace Network {

template<typename Session>
class TcpServer : public mINI {
public:
	typedef std::shared_ptr<TcpServer> Ptr;
	TcpServer() {
		socket.reset(new Socket());
		sessionMap.reset(new typename decltype(sessionMap)::element_type);
	}

	~TcpServer() {
		TraceL << "start clean...";
		timer.reset();
		socket.reset();

		typename decltype(sessionMap)::element_type copyMap;
		sessionMap->swap(copyMap);
		for (auto it = copyMap.begin(); it != copyMap.end(); ++it) {
			auto session = it->second;
			it->second->async_first( [session]() {
				session->onError(SockException(Err_other,"Tcp server shutdown!"));
			});
		}
		TraceL << "clean completed!";
	}
	void start(uint16_t port, const std::string& host = "0.0.0.0", uint32_t backlog = 1024) {
		bool success = socket->listen(port, host.c_str(), backlog);
		if (!success) {
			string err = (StrPrinter << "listen on " << host << ":" << port << "] failed:" << get_uv_errmsg(true)).operator <<(endl);
			throw std::runtime_error(err);
		}
		socket->setOnAccept( bind(&TcpServer::onAcceptConnection, this, placeholders::_1));
		timer.reset(new Timer(2, [this]()->bool {
			this->onManagerSession();
			return true;
		}));
		InfoL << "TCP Server listening on " << host << ":" << port;
	}

private:
	Socket::Ptr socket;
	std::shared_ptr<Timer> timer;
	std::shared_ptr<std::unordered_map<Socket *, std::shared_ptr<Session> > > sessionMap;

	void onAcceptConnection(const Socket::Ptr & sock) {
		// 接收到客户端连接请求
		auto session(std::make_shared<Session>(WorkThreadPool::Instance().getWorkThread(), sock));
		session->attachServer(*this);
		auto sockPtr(sock.get());
		auto sessionMapTmp(sessionMap);
		weak_ptr<Session> weakSession(session);
		sessionMapTmp->emplace(sockPtr, session);

		// 会话接收数据事件
		sock->setOnRead([weakSession](const Socket::Buffer::Ptr &buf, struct sockaddr *addr){
			//获取会话强应用
			auto strongSession=weakSession.lock();
			if(!strongSession) {
				//会话对象已释放
				return;
			}
			//在会话线程中执行onRecv操作
			strongSession->async([weakSession,buf]() {
				auto strongSession=weakSession.lock();
				if(!strongSession) {
					return;
				}
				strongSession->onRecv(buf);
			});
		});

		//会话接收到错误事件
		sock->setOnErr([weakSession,sockPtr,sessionMapTmp](const SockException &err){
			//获取会话强应用
			auto strongSession=weakSession.lock();
			//移除掉会话
			sessionMapTmp->erase(sockPtr);
			if(!strongSession) {
				//会话对象已释放
				return;
			}
			//在会话线程中执行onError操作
			strongSession->async_first([strongSession,err]() {
				strongSession->onError(err);
			});
		});
	}

	void onManagerSession() {
		//DebugL<<EventPoller::Instance().isMainThread();
		for (auto &pr : *sessionMap) {
			weak_ptr<Session> weakSession = pr.second;
			pr.second->async([weakSession]() {
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
