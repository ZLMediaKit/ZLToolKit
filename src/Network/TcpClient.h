/*
 * TcpClient.h
 *
 *  Created on: 2017年2月13日
 *      Author: xzl
 */

#ifndef SRC_NETWORK_TCPCLIENT_H_
#define SRC_NETWORK_TCPCLIENT_H_

#include <memory>
#include <functional>
#include "Socket.hpp"
#include "Util/TimeTicker.h"

using namespace std;
using namespace ZL::Util;

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
	int send(const string &str);
	int send(const char *str, int len);
	bool alive() {
		return m_pSock.operator bool();
	}
	string get_local_ip() {
		if (!m_pSock) {
			return "";
		}
		return m_pSock->get_local_ip();
	}
	uint16_t get_local_port() {
		if (!m_pSock) {
			return 0;
		}
		return m_pSock->get_local_port();
	}
	string get_peer_ip() {
		if (!m_pSock) {
			return "";
		}
		return m_pSock->get_peer_ip();
	}
	uint16_t get_peer_port() {
		if (!m_pSock) {
			return 0;
		}
		return m_pSock->get_peer_port();
	}

	uint64_t elapsedTime();

	virtual void onConnect(const SockException &ex) {}
	virtual void onRecv(const Socket::Buffer::Ptr &pBuf) {}
	virtual void onSend() {}
	virtual void onErr(const SockException &ex) {}
private:
	Socket::Ptr m_pSock;
	Ticker m_ticker;
	void onSockConnect(const SockException &ex);
	void onSockRecv(const Socket::Buffer::Ptr &pBuf);
	void onSockSend();
	void onSockErr(const SockException &ex);

};

} /* namespace Network */
} /* namespace ZL */

#endif /* SRC_NETWORK_TCPCLIENT_H_ */
