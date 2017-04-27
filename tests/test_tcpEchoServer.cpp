//============================================================================
// Name        : ToolKitTest.cpp
// Author      : xzl
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <signal.h>
#include <iostream>
#include "Util/logger.h"
#include "Network/TcpServer.h"
#include "Network/TcpSession.h"
using namespace std;
using namespace ZL::Util;
using namespace ZL::Network;

class EchoSession: public TcpSession {
public:
	EchoSession(const std::shared_ptr<ThreadPool> &th,
			const Socket::Ptr &sock) :
			TcpSession(th, sock) {
	}
	virtual ~EchoSession() {
		DebugL;
	}
	virtual void onRecv(const Socket::Buffer::Ptr &buf) override{
		TraceL << buf->data();
		send(buf->data(),buf->size());
	}
	virtual void onError(const SockException &err) override{
		WarnL << err.what();
	}
	virtual void onManager() {
		DebugL;
	}
};
void programExit(int arg) {
	EventPoller::Instance().shutdown();
}
int main() {
	//测试方法：先启动test_tcpEchoServer，在启动test_tcpClient
	signal(SIGINT, programExit);
	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
	Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

	TcpServer<EchoSession>::Ptr server(new TcpServer<EchoSession>());
	server->start(9000);

	EventPoller::Instance().runLoop();

	server.reset();
	//TcpServer依赖线程池 需要销毁
	WorkThreadPool::Destory();
	EventPoller::Destory();
	Logger::Destory();
	return 0;
}






