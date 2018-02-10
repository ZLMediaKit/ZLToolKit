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

#include <memory>
#include <functional>
#include "Socket.h"
#include "Util/TimeTicker.h"
#include "Thread/WorkThreadPool.h"
#include "Thread/spin_mutex.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Thread;

namespace ZL {
namespace Network {


//Tcp客户端，接口非线程安全的，建议切换到主线程后再操作该对象
class TcpClient : public std::enable_shared_from_this<TcpClient> , public SocketWriter{
public:
	typedef std::shared_ptr<TcpClient> Ptr;
	TcpClient();
	virtual ~TcpClient();
    //开始连接服务器，strUrl可以是域名或ip
    void startConnect(const string &strUrl, uint16_t iPort, float fTimeOutSec = 3);
    //主动断开服务器
    void shutdown();
    //是否与服务器连接中
    bool alive();
protected:
    //发送数据
	virtual int send(const string &str);
    virtual int send(string &&buf);
	virtual int send(const char *str, int len);
	virtual int send(const Buffer::Ptr &buf);
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
    //从socket缓存池中获取一片缓存，如果未连接，则返回空
    BufferRaw::Ptr obtainBuffer();

    /////////获取ip或端口///////////
	string get_local_ip();
    uint16_t get_local_port();
	string get_peer_ip();
	uint16_t get_peer_port();
private:
	void onSockConnect(const SockException &ex);
	void onSockRecv(const Buffer::Ptr &pBuf);
	void onSockSend();
	void onSockErr(const SockException &ex);
private:
    spin_mutex _mutex;
    Socket::Ptr _sock;
    std::shared_ptr<Timer> _managerTimer;
};

} /* namespace Network */
} /* namespace ZL */

#endif /* NETWORK_TCPCLIENT_H */
