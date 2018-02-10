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
#include <signal.h>
#include <iostream>
#include "Util/util.h"
#include "Util/logger.h"
#include "Network/Socket.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Network;


//主线程退出标志
bool exitProgram = false;

//赋值struct sockaddr
void makeAddr(struct sockaddr *out,const char *ip,uint16_t port){
	struct sockaddr_in &servaddr = *((struct sockaddr_in *)out);
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = inet_addr(ip);
	bzero(&(servaddr.sin_zero), sizeof servaddr.sin_zero);
}

//获取struct sockaddr的IP字符串
string getIP(struct sockaddr *addr){
	return inet_ntoa(((struct sockaddr_in *)addr)->sin_addr);
}
int main() {
    //设置程序退出信号处理函数
	signal(SIGINT, [](int){exitProgram = true;});
	EventPoller::Instance(true);//主线程为自轮询类型，不需要调用runLoop
    //设置日志系统
	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
	Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

	Socket::Ptr sockRecv(new Socket);//创建一个UDP数据接收端口
	Socket::Ptr sockSend(new Socket);//创建一个UDP数据发送端口
	sockRecv->bindUdpSock(9001);//接收UDP绑定9001端口
	sockSend->bindUdpSock(0);//发送UDP随机端口

	sockRecv->setOnRead([](const Buffer::Ptr &buf, struct sockaddr *addr){
        //接收到数据回调
		DebugL << "recv data form " << getIP(addr) << ":" << buf->data();
	});

	struct sockaddr addrDst;
	makeAddr(&addrDst,"127.0.0.1",9001);//UDP数据发送地址

	int i = 0;
	while(!exitProgram){
        //每隔一秒往对方发送数据
		sockSend->send(to_string(i++),SOCKET_DEFAULE_FLAGS,&addrDst);
		sleep(1);
	}

    //程序开始推出，做些清理工作
	EventPoller::Destory();
	Logger::Destory();
	return 0;
}





