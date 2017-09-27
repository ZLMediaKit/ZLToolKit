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
	EchoSession(const std::shared_ptr<ThreadPool> &th, const Socket::Ptr &sock) :
			TcpSession(th, sock) {
	}
	virtual ~EchoSession() {
		DebugL;
	}
	virtual void onRecv(const Socket::Buffer::Ptr &buf) override{
		//处理客户端发送过来的数据
		TraceL << buf->data();
		//把数据回显至客户端
		send(buf->data(),buf->size());
	}
	virtual void onError(const SockException &err) override{
		//客户端断开连接或其他原因导致该对象脱离TCPServer管理
		WarnL << err.what();
	}
	virtual void onManager() {
		//定时管理该对象，譬如会话超时检查
		DebugL;
	}
};


int main() {
	//退出程序事件处理
	signal(SIGINT, [](int){EventPoller::Instance().shutdown();});
	//初始化日志模块
	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
	Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

	TcpServer<EchoSession>::Ptr server(new TcpServer<EchoSession>());
	server->start(9000);//监听9000端口

	EventPoller::Instance().runLoop();//主线程事件轮询

	server.reset();//销毁服务器
	//TcpServer 依赖线程池，需要销毁
	WorkThreadPool::Destory();
	EventPoller::Destory();
	Logger::Destory();
	return 0;
}