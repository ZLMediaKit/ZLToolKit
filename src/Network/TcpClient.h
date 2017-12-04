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

#ifndef SRC_NETWORK_TCPCLIENT_H_
#define SRC_NETWORK_TCPCLIENT_H_

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

class TcpClient : public std::enable_shared_from_this<TcpClient> {
public:
	typedef std::shared_ptr<TcpClient> Ptr;
	TcpClient();
	virtual ~TcpClient();
protected:
	void startConnect(const string &strUrl, uint16_t iPort, int iTimeOutSec = 3);
	void shutdown();
	virtual int send(const string &str);
    virtual int send(string &&buf);
	virtual int send(const char *str, int len);
	bool alive() {
		lock_guard<spin_mutex> lck(m_mutex);
		return m_pSock.operator bool();
	}
	string get_local_ip() {
		decltype(m_pSock) sockTmp;
		{
			lock_guard<spin_mutex> lck(m_mutex);
			sockTmp = m_pSock;
		}
		if(!sockTmp){
			return "";
		}
		return sockTmp->get_local_ip();
	}
	uint16_t get_local_port() {
		decltype(m_pSock) sockTmp;
		{
			lock_guard<spin_mutex> lck(m_mutex);
			sockTmp = m_pSock;
		}
		if(!sockTmp){
			return 0;
		}
		return sockTmp->get_local_port();
	}
	string get_peer_ip() {
		decltype(m_pSock) sockTmp;
		{
			lock_guard<spin_mutex> lck(m_mutex);
			sockTmp = m_pSock;
		}
		if(!sockTmp){
			return "";
		}
		return sockTmp->get_peer_ip();
	}
	uint16_t get_peer_port() {
		decltype(m_pSock) sockTmp;
		{
			lock_guard<spin_mutex> lck(m_mutex);
			sockTmp = m_pSock;
		}
		if(!sockTmp){
			return 0;
		}
		return sockTmp->get_peer_port();
	}

	uint64_t elapsedTime();

	//链接成功后，客户端将绑定一个后台线程，并且onConnect/onRecv/onSend/onErr事件将在该后台线程触发
	virtual void onConnect(const SockException &ex) {}
	virtual void onRecv(const Socket::Buffer::Ptr &pBuf) {}
	virtual void onSend() {}
	virtual void onErr(const SockException &ex) {}
	Socket::Ptr m_pSock;
private:
	Ticker m_ticker;
	spin_mutex m_mutex;

	void onSockConnect(const SockException &ex);
	void onSockRecv(const Socket::Buffer::Ptr &pBuf);
	void onSockSend();
	void onSockErr(const SockException &ex);

};

} /* namespace Network */
} /* namespace ZL */

#endif /* SRC_NETWORK_TCPCLIENT_H_ */
