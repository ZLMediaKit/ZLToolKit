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
	auto sock = m_pSock;
	if(sock){
		sock->setOnErr(nullptr);
		sock->setOnFlush(nullptr);
		sock->setOnRead(nullptr);
	}
	m_pSock.reset();
}
void TcpClient::startConnect(const string &strUrl, uint16_t iPort,int iTimeoutSec) {
	shutdown();
	Socket::Ptr sock(new Socket());
	weak_ptr<TcpClient> weakSelf = shared_from_this();
	sock->connect(strUrl, iPort, [weakSelf](const SockException &err){
		auto strongSelf = weakSelf.lock();
		if(strongSelf){
			strongSelf->onSockConnect(err);
		}
	}, iTimeoutSec);
	m_ticker.resetTime();
	m_pSock = sock;
}
void TcpClient::onSockConnect(const SockException &ex) {
	onConnect(ex);
	if(!ex){
		auto sock = m_pSock;
		if (sock) {
			weak_ptr<TcpClient> weakSelf = shared_from_this();
			sock->setOnErr([weakSelf](const SockException &ex) {
				auto strongSelf = weakSelf.lock();
				if(strongSelf) {
					strongSelf->onSockErr(ex);
				}
			});
			sock->setOnFlush([weakSelf]() {
				auto strongSelf = weakSelf.lock();
				if(strongSelf) {
					strongSelf->onSockSend();
				}
				return strongSelf.operator bool();
			});
			sock->setOnRead( [weakSelf](const Socket::Buffer::Ptr &pBuf, struct sockaddr *addr) {
				auto strongSelf = weakSelf.lock();
				if(strongSelf) {
					strongSelf->onSockRecv(pBuf);
				}
			});
		}
	}
}

void TcpClient::onSockRecv(const Socket::Buffer::Ptr& pBuf) {
	m_ticker.resetTime();
	onRecv(pBuf);
}

void TcpClient::onSockSend() {
	m_ticker.resetTime();
	onSend();
}

void TcpClient::onSockErr(const SockException& ex) {
	onErr(ex);
	shutdown();
}

int TcpClient::send(const string& str) {
	m_ticker.resetTime();
	auto sock = m_pSock;
	if(sock){
		return sock->send(str);
	}
	return -1;
}

int TcpClient::send(const char* str, int len) {
	m_ticker.resetTime();
	auto sock = m_pSock;
	if (sock) {
		return sock->send(str, len);
	}
	return -1;
}

uint64_t TcpClient::elapsedTime() {
	return m_ticker.elapsedTime();
}

} /* namespace Network */
} /* namespace ZL */
