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
#include "TcpClient.h"

namespace ZL {
namespace Network {

TcpClient::TcpClient() {
}

TcpClient::~TcpClient() {
	shutdown();
}

void TcpClient::shutdown() {
	decltype(m_pSock) sockTmp;
	{
		lock_guard<spin_mutex> lck(m_mutex);
		sockTmp = m_pSock;
	}
	if(sockTmp){
		sockTmp->setOnErr(nullptr);
		sockTmp->setOnFlush(nullptr);
		sockTmp->setOnRead(nullptr);
	}
	lock_guard<spin_mutex> lck(m_mutex);
	m_pSock.reset();
}
void TcpClient::startConnect(const string &strUrl, uint16_t iPort,int iTimeoutSec) {
	shutdown();
	Socket::Ptr sockTmp(new Socket());
	{
		lock_guard<spin_mutex> lck(m_mutex);
		m_ticker.resetTime();
		m_pSock = sockTmp;
	}

	weak_ptr<TcpClient> weakSelf = shared_from_this();
	sockTmp->connect(strUrl, iPort, [weakSelf](const SockException &err){
		auto strongSelf = weakSelf.lock();
		if(strongSelf){
            if(err){
            	lock_guard<spin_mutex> lck(strongSelf->m_mutex);
                strongSelf->m_pSock.reset();
            }
			strongSelf->onSockConnect(err);
		}
	}, iTimeoutSec);
}
void TcpClient::onSockConnect(const SockException &ex) {
#if defined(ENABLE_ASNC_TCP_CLIENT)
	auto threadTmp = WorkThreadPool::Instance().getWorkThread();
#else
	auto threadTmp = &EventPoller::Instance();
#endif//ENABLE_ASNC_TCP_CLIENT

	decltype(m_pSock) sockTmp;
	{
		lock_guard<spin_mutex> lck(m_mutex);
		sockTmp = m_pSock;
	}
	weak_ptr<TcpClient> weakSelf = shared_from_this();
	if(!ex && sockTmp) {
		sockTmp->setOnErr([weakSelf, threadTmp](const SockException &ex) {
			auto strongSelf = weakSelf.lock();
			if (!strongSelf) {
				return;
			}
			threadTmp->async([weakSelf, ex]() {
				auto strongSelf = weakSelf.lock();
				if (!strongSelf) {
					return;
				}
				strongSelf->onSockErr(ex);
			});
		});
		sockTmp->setOnFlush([weakSelf, threadTmp]() {
			auto strongSelf = weakSelf.lock();
			if (!strongSelf) {
				return false;
			}
			threadTmp->async([weakSelf]() {
				auto strongSelf = weakSelf.lock();
				if (!strongSelf) {
					return;
				}
				strongSelf->onSockSend();
			});
			return true;
		});
		sockTmp->setOnRead([weakSelf, threadTmp](const Socket::Buffer::Ptr &pBuf, struct sockaddr *addr) {
			auto strongSelf = weakSelf.lock();
			if (!strongSelf) {
				return;
			}
			threadTmp->async([weakSelf, pBuf]() {
				auto strongSelf = weakSelf.lock();
				if (!strongSelf) {
					return;
				}
				strongSelf->onSockRecv(pBuf);
			});
		});
	}
	threadTmp->async([weakSelf,ex](){
		auto strongSelf = weakSelf.lock();
		if(strongSelf){
			strongSelf->onConnect(ex);
		}
	});
}

void TcpClient::onSockRecv(const Socket::Buffer::Ptr& pBuf) {
	{
		lock_guard<spin_mutex> lck(m_mutex);
		m_ticker.resetTime();
	}
	onRecv(pBuf);
}

void TcpClient::onSockSend() {
	{
		lock_guard<spin_mutex> lck(m_mutex);
		m_ticker.resetTime();
	}
	onSend();
}

void TcpClient::onSockErr(const SockException& ex) {
	shutdown();
	onErr(ex);
}

int TcpClient::send(const string& str) {
	decltype(m_pSock) sockTmp;
	{
		lock_guard<spin_mutex> lck(m_mutex);
		sockTmp = m_pSock;
		m_ticker.resetTime();
	}
	if (sockTmp) {
		return sockTmp->send(str);
	}
	return -1;
}
int TcpClient::send(string&& str) {
	decltype(m_pSock) sockTmp;
	{
		lock_guard<spin_mutex> lck(m_mutex);
		sockTmp = m_pSock;
		m_ticker.resetTime();
	}
	if (sockTmp) {
		return sockTmp->send(std::move(str));
	}
	return -1;
}
int TcpClient::send(const char* str, int len) {
	decltype(m_pSock) sockTmp;
	{
		lock_guard<spin_mutex> lck(m_mutex);
		sockTmp = m_pSock;
		m_ticker.resetTime();
	}
	if (sockTmp) {
		return sockTmp->send(str, len);
	}
	return -1;
}

uint64_t TcpClient::elapsedTime() {
	lock_guard<spin_mutex> lck(m_mutex);
	return m_ticker.elapsedTime();
}

} /* namespace Network */
} /* namespace ZL */
