/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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
using namespace toolkit;

class TestClient: public TcpClient {
public:
	typedef std::shared_ptr<TestClient> Ptr;
	TestClient():TcpClient() {
		DebugL;
	}
	~TestClient(){
		DebugL;
	}
protected:
	virtual void onConnect(const SockException &ex) override{
		//连接结果事件
		InfoL << (ex ?  ex.what() : "success");
	}
	virtual void onRecv(const Buffer::Ptr &pBuf) override{
		//接收数据事件
		DebugL << pBuf->data() << " from port:" << get_peer_port();
	}
	virtual void onSend() override{
		//发送阻塞后，缓存清空事件
		DebugL;
	}
	virtual void onErr(const SockException &ex) override{
		//断开连接事件，一般是EOF
		WarnL << ex.what();
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
    // 设置日志系统
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

	TestClient::Ptr client(new TestClient());//必须使用智能指针
	client->startConnect("127.0.0.1",9000);//连接服务器

	TcpClientWithSSL<TestClient>::Ptr clientSSL(new TcpClientWithSSL<TestClient>());//必须使用智能指针
	clientSSL->startConnect("127.0.0.1",9001);//连接服务器

	//退出程序事件处理
	static semaphore sem;
	signal(SIGINT, [](int) { sem.post(); });// 设置退出信号
	sem.wait();
	return 0;
}