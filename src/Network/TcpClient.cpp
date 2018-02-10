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

TcpClient::TcpClient() : SocketWriter(nullptr) {
}

TcpClient::~TcpClient() {
	shutdown();
}

void TcpClient::shutdown() {
    lock_guard<spin_mutex> lck(_mutex);
    if(_sock){
        _sock->setOnErr(nullptr);
        _sock->setOnFlush(nullptr);
        _sock->setOnRead(nullptr);
        _sock.reset();
    }
    SocketWriter::setSock(nullptr);
    _managerTimer.reset();
}

void TcpClient::startConnect(const string &strUrl, uint16_t iPort,float fTimeOutSec) {
	shutdown();

    lock_guard<spin_mutex> lck(_mutex);
    _sock.reset(new Socket());
    SocketWriter::setSock(_sock);

    weak_ptr<TcpClient> weakSelf = shared_from_this();
    _sock->connect(strUrl, iPort, [weakSelf](const SockException &err){
		auto strongSelf = weakSelf.lock();
		if(strongSelf){
            if(err){
            	lock_guard<spin_mutex> lck(strongSelf->_mutex);
                strongSelf->_sock.reset();
            }
			strongSelf->onSockConnect(err);
		}
	}, fTimeOutSec);
}
void TcpClient::onSockConnect(const SockException &ex) {
    lock_guard<spin_mutex> lck(_mutex);
	if(!ex && _sock) {
        weak_ptr<TcpClient> weakSelf = shared_from_this();
        _sock->setOnErr([weakSelf](const SockException &ex) {
			auto strongSelf = weakSelf.lock();
			if (!strongSelf) {
				return;
			}
            strongSelf->onSockErr(ex);
		});
        _sock->setOnFlush([weakSelf]() {
			auto strongSelf = weakSelf.lock();
			if (!strongSelf) {
				return false;
			}
            strongSelf->onSockSend();
			return true;
		});
        _sock->setOnRead([weakSelf](const Buffer::Ptr &pBuf, struct sockaddr *addr) {
			auto strongSelf = weakSelf.lock();
			if (!strongSelf) {
				return;
			}
            strongSelf->onSockRecv(pBuf);
		});
        _managerTimer.reset(new Timer(2,[weakSelf](){
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return false;
            }
            strongSelf->onManager();
            return true;
        }));
	}
    onConnect(ex);
}

void TcpClient::onSockRecv(const Buffer::Ptr& pBuf) {
	onRecv(pBuf);
}

void TcpClient::onSockSend() {
	onSend();
}

void TcpClient::onSockErr(const SockException& ex) {
	shutdown();
	onErr(ex);
}

int TcpClient::send(const string& str) {
    lock_guard<spin_mutex> lck(_mutex);
	if (_sock) {
		return _sock->send(str,_flags);
	}
	return -1;
}
int TcpClient::send(string&& str) {
    lock_guard<spin_mutex> lck(_mutex);
	if (_sock) {
		return _sock->send(std::move(str),_flags);
	}
	return -1;
}
int TcpClient::send(const char* str, int len) {
    lock_guard<spin_mutex> lck(_mutex);
	if (_sock) {
		return _sock->send(str, len,_flags);
	}
	return -1;
}

int TcpClient::send(const Buffer::Ptr &buf){
    lock_guard<spin_mutex> lck(_mutex);
	if (_sock) {
		return _sock->send(buf,_flags);
	}
	return -1;
}

bool TcpClient::alive() {
    lock_guard<spin_mutex> lck(_mutex);
    return _sock.operator bool();
}
//从缓存池中获取一片缓存
BufferRaw::Ptr TcpClient::obtainBuffer(){
    lock_guard<spin_mutex> lck(_mutex);
    if(!_sock){
        return nullptr;
    }
    return _sock->obtainBuffer();
}
string TcpClient::get_local_ip() {
    lock_guard<spin_mutex> lck(_mutex);
    if(!_sock){
        return "";
    }
    return _sock->get_local_ip();
}
uint16_t TcpClient::get_local_port() {
    lock_guard<spin_mutex> lck(_mutex);
    if(!_sock){
        return 0;
    }
    return _sock->get_local_port();
}
string TcpClient::get_peer_ip() {
    lock_guard<spin_mutex> lck(_mutex);
    if(!_sock){
        return "";
    }
    return _sock->get_peer_ip();
}
uint16_t TcpClient::get_peer_port() {
    lock_guard<spin_mutex> lck(_mutex);
    if(!_sock){
        return 0;
    }
    return _sock->get_peer_port();
}

} /* namespace Network */
} /* namespace ZL */
