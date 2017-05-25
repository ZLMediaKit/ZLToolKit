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
#include "Network/TcpClient.h"
using namespace std;
using namespace ZL::Util;
using namespace ZL::Network;


class TestClient: public TcpClient {
public:
	typedef std::shared_ptr<TestClient> Ptr;
	TestClient() {
	}
	virtual ~TestClient(){}
	void connect(){
		startConnect("127.0.0.1",9000);
	}
protected:
	virtual void onConnect(const SockException &ex) override{
		InfoL << (ex ? "success" : ex.what());
		if(!ex){
			weak_ptr<TestClient> weakSelf = dynamic_pointer_cast<TestClient>(shared_from_this());
			_timer.reset(new Timer(1,[weakSelf](){
				auto strongSelf = weakSelf.lock();
				if(strongSelf){
					strongSelf->onTick();
				}
				return strongSelf.operator bool();
			}));
		}
	}
	virtual void onRecv(const Socket::Buffer::Ptr &pBuf) override{
		DebugL << pBuf->data();
	}
	virtual void onSend() override{
		DebugL;
	}
	virtual void onErr(const SockException &ex) override{
		WarnL << ex.what();
		_timer.reset();
	}
	void onTick(){
		send(to_string(_nTick++));
	}
	int _nTick = 0;
	std::shared_ptr<Timer> _timer;
};
void programExit(int arg) {
	EventPoller::Instance().shutdown();
}
int main() {
	//测试方法：先启动test_tcpEchoServer，在启动test_tcpClient
	signal(SIGINT, programExit);
	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
	Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

	TestClient::Ptr client(new TestClient());
	client->connect();
	EventPoller::Instance().runLoop();
	EventPoller::Destory();
	Logger::Destory();
	return 0;
}

