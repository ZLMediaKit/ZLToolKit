//============================================================================
// Name        : ZLToolKit.cpp
// Author      : 熊子良
// Version     :
// Copyright   : 本代码为熊子良自己维护使用，可以自由使用、修改、发布，版权归熊子良所有。
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <signal.h>
#include <unistd.h>
#include <Util/logger.h>
#include <iostream>
#include "Util/TimeTicker.h"
#include "Util/recyclePool.h"
#include "Thread/ThreadPool.hpp"
#include "Util/SSLBox.h"
#include "Util/SqlPool.h"
#include "Network/Socket.hpp"
#include "Network/TcpServer.h"
#include "Poller/EventPoller.hpp"
#include "Session/Session.h"
using namespace std;
using namespace ZL::Util;
using namespace ZL::Thread;
using namespace ZL::Network;
using namespace ZL::Session;

TcpServer<Session> *tcpServer;

void programExit(int arg) {
	if (tcpServer) {
		delete tcpServer;
		tcpServer = nullptr;
	}
	EventPoller::Instance().shutdown();
}
int main() {
	Logger::instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
	Logger::instance().setWriter(std::make_shared<AsyncLogWriter>());
	tcpServer = new TcpServer<Session>();
	tcpServer->start(8000);
	signal(SIGINT, programExit);
	EventPoller::Instance().runLoop();
	return 0;
}
