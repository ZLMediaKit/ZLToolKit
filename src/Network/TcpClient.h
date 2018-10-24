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

#ifndef NETWORK_TCPCLIENT_H
#define NETWORK_TCPCLIENT_H

#include <mutex>
#include <memory>
#include <functional>
#include "Socket.h"
#include "Util/TimeTicker.h"
#include "Thread/WorkThreadPool.h"

using namespace std;

namespace toolkit {


//Tcp客户端，接口线程安全的
class TcpClient :
        public std::enable_shared_from_this<TcpClient> ,
        public SocketHelper{
public:
	typedef std::shared_ptr<TcpClient> Ptr;
	TcpClient(const EventPoller::Ptr &poller = nullptr,
              const TaskExecutor::Ptr &executor = nullptr);
	virtual ~TcpClient();
    //开始连接服务器，strUrl可以是域名或ip
    void startConnect(const string &strUrl, uint16_t iPort, float fTimeOutSec = 3);
    //主动断开服务器
    void shutdown() override ;
    //是否与服务器连接中
    bool alive();
    //设置网卡适配器
    void setNetAdapter(const string &localIp);
protected:
    //连接服务器结果回调
    virtual void onConnect(const SockException &ex) {}
    //收到数据回调
    virtual void onRecv(const Buffer::Ptr &pBuf) {}
    //数据全部发送完毕后回调
    virtual void onSend() {}
    //被动断开连接回调
    virtual void onErr(const SockException &ex) {}
    //tcp连接成功后每2秒触发一次该事件
    virtual void onManager() {}
private:
	void onSockConnect(const SockException &ex);
	void onSockRecv(const Buffer::Ptr &pBuf);
	void onSockSend();
	void onSockErr(const SockException &ex);
private:
    EventPoller::Ptr _poller;
    TaskExecutor::Ptr _executor;
    std::shared_ptr<Timer> _managerTimer;
    string _netAdapter = "0.0.0.0";
};

} /* namespace toolkit */

#endif /* NETWORK_TCPCLIENT_H */
