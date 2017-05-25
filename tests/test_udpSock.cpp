//============================================================================
// Name        : ToolKitTest.cpp
// Author      : xzl
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <iostream>
#include "Util/logger.h"
#include "Network/Socket.h"
using namespace std;
using namespace ZL::Util;
using namespace ZL::Network;


bool exitProgram = false;
void programExit(int arg) {
	exitProgram = true;
}

void makeAddr(struct sockaddr *out,const char *ip,uint16_t port){
	struct sockaddr_in &servaddr = *((struct sockaddr_in *)out);
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = inet_addr(ip);
	bzero(&(servaddr.sin_zero), sizeof servaddr.sin_zero);
}
string getIP(struct sockaddr *addr){
	return inet_ntoa(((struct sockaddr_in *)addr)->sin_addr);
}
int main() {
	signal(SIGINT, programExit);
	EventPoller::Instance(true);

	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
	Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

	Socket::Ptr sockRecv(new Socket);
	Socket::Ptr sockSend(new Socket);
	sockRecv->bindUdpSock(9001);
	sockSend->bindUdpSock(0);
	sockRecv->setOnRead([](const Socket::Buffer::Ptr &buf, struct sockaddr *addr){
		DebugL << "recv data form " << getIP(addr) << ":" << buf->data();
	});

	struct sockaddr addrDst;
	makeAddr(&addrDst,"127.0.0.1",9001);

	int i = 0;
	while(!exitProgram){
		sockSend->sendTo(to_string(i++),&addrDst);
		sleep(1);
	}

	EventPoller::Destory();
	Logger::Destory();
	return 0;
}





