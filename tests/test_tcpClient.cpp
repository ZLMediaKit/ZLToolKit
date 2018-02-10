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
		//这里改成实际服务器地址
		startConnect("127.0.0.1",9000);
	}
protected:
	virtual void onConnect(const SockException &ex) override{
		//连接结果事件
		InfoL << (ex ?  ex.what() : "success");
	}
	virtual void onRecv(const Buffer::Ptr &pBuf) override{
		//接收数据事件
		DebugL << pBuf->data();
	}
	virtual void onSend() override{
		//发送阻塞后，缓存清空事件
		DebugL;
	}
	virtual void onErr(const SockException &ex) override{
		//断开连接事件，一般是EOF
		WarnL << ex.what();
		connect();
	}
    virtual void onManager() override{
		//定时发送数据到服务器
        BufferRaw::Ptr buf = obtainBuffer();
        if(buf){
            buf->assign("[BufferRaw]\0");
            (*this) << SocketFlags(SOCKET_DEFAULE_FLAGS | FLAG_MORE)
                    << _nTick++ << " "
                    << 3.14 << " "
                    << string("string") << " "
                    <<(Buffer::Ptr &)buf;
        }
	}
private:
	int _nTick = 0;
};


int main() {
    signal(SIGINT, [](int) { EventPoller::Instance().shutdown(); });// 设置退出信号
    // 设置日志系统
    Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    {
        TestClient::Ptr client(new TestClient());//必须使用智能指针
        client->connect();//连接服务器
        EventPoller::Instance().runLoop();//主线程事件轮询
    }

	EventPoller::Destory();
	Logger::Destory();
	return 0;
}