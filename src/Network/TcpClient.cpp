/*
 * TcpClient.cpp
 *
 *  Created on: 2017年2月13日
 *      Author: xzl
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
#ifdef ENABLE_ASNC_TCP_CLIENT
	auto threadTmp = WorkThreadPool::Instance().getWorkThread();
#else
	auto threadTmp = &EventPoller::Instance();
#endif//ENABLE_ASNC_TCP_CLIENT
	weak_ptr<TcpClient> weakSelf = shared_from_this();
	threadTmp->async([weakSelf,ex](){
		auto strongSelf = weakSelf.lock();
		if(strongSelf){
			strongSelf->onConnect(ex);
		}
	});
	decltype(m_pSock) sockTmp;
	{
		lock_guard<spin_mutex> lck(m_mutex);
		sockTmp = m_pSock;
	}
	if(ex || !sockTmp){
		return;
	}
	sockTmp->setOnErr([weakSelf,threadTmp](const SockException &ex) {
		auto strongSelf = weakSelf.lock();
		if(!strongSelf) {
			return;
		}
		threadTmp->async([weakSelf,ex](){
			auto strongSelf = weakSelf.lock();
			if(!strongSelf) {
				return;
			}
			strongSelf->onSockErr(ex);
		});
	});
	sockTmp->setOnFlush([weakSelf,threadTmp]() {
		auto strongSelf = weakSelf.lock();
		if(!strongSelf) {
			return false;
		}
		threadTmp->async([weakSelf](){
			auto strongSelf = weakSelf.lock();
			if(!strongSelf) {
				return;
			}
			strongSelf->onSockSend();
		});
		return true;
	});
	sockTmp->setOnRead([weakSelf,threadTmp](const Socket::Buffer::Ptr &pBuf, struct sockaddr *addr) {
		auto strongSelf = weakSelf.lock();
		if(!strongSelf) {
			return;
		}
		threadTmp->async([weakSelf,pBuf](){
			auto strongSelf = weakSelf.lock();
			if(!strongSelf) {
				return;
			}
			strongSelf->onSockRecv(pBuf);
		});
	});
}

void TcpClient::onSockRecv(const Socket::Buffer::Ptr& pBuf) {
	onRecv(pBuf);
	lock_guard<spin_mutex> lck(m_mutex);
	m_ticker.resetTime();
}

void TcpClient::onSockSend() {
	onSend();
	lock_guard<spin_mutex> lck(m_mutex);
	m_ticker.resetTime();
}

void TcpClient::onSockErr(const SockException& ex) {
	onErr(ex);
	shutdown();
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
