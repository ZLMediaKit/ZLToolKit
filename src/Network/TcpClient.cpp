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

namespace toolkit {

TcpClient::TcpClient(const EventPoller::Ptr &poller,
                     const TaskExecutor::Ptr &executor) : SocketHelper(nullptr) {
    _poller = poller;
    if(!_poller){
        _poller = EventPollerPool::Instance().getPoller();
    }
    _executor = executor;
    if(!_executor){
        _executor = _poller;
    }
    setExecutor(_executor);
}

TcpClient::~TcpClient() {
}

void TcpClient::shutdown() {
    try {
        weak_ptr<TcpClient> weakSelf = shared_from_this();
        async([weakSelf](){
            auto strongSelf = weakSelf.lock();
            if(!strongSelf){
                return;
            }
            strongSelf->setSock(nullptr);
            strongSelf->_managerTimer.reset();
        });
    }catch (std::exception &ex){
        //ErrorL << "catch exception:" << ex.what();
    }
}

bool TcpClient::alive() {
    bool ret = _sock.operator bool();
    return ret;
}

void TcpClient::setNetAdapter(const string &localIp){
    weak_ptr<TcpClient> weakSelf = shared_from_this();
    async([weakSelf,localIp](){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            return;
        }
        strongSelf->_netAdapter = localIp;
    });
}


void TcpClient::startConnect(const string &strUrl, uint16_t iPort,float fTimeOutSec) {
    weak_ptr<TcpClient> weakSelf = shared_from_this();
    async([strUrl,iPort,fTimeOutSec,weakSelf,this](){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            return;
        }
        shutdown();
        SocketHelper::setSock(std::make_shared<Socket>(_poller,_executor));

        weak_ptr<TcpClient> weakSelf = shared_from_this();
        _sock->connect(strUrl, iPort, [weakSelf](const SockException &err){
            auto strongSelf = weakSelf.lock();
            if(strongSelf){
                if(err){
                    strongSelf->SocketHelper::setSock(nullptr);
                }
                strongSelf->onSockConnect(err);
            }
        }, fTimeOutSec,_netAdapter.data(),0);
    });
}
void TcpClient::onSockConnect(const SockException &ex) {
	if(!ex && _sock) {
        weak_ptr<TcpClient> weakSelf = shared_from_this();
        auto sock_ptr = _sock.get();
        _sock->setOnErr([weakSelf,sock_ptr](const SockException &ex) {
			auto strongSelf = weakSelf.lock();
			if (!strongSelf) {
				return;
			}
			if(sock_ptr != strongSelf->_sock.get()){
                return;
			}
            strongSelf->onSockErr(ex);
		});
        _sock->setOnFlush([weakSelf,sock_ptr]() {
			auto strongSelf = weakSelf.lock();
			if (!strongSelf) {
				return false;
			}
            if(sock_ptr != strongSelf->_sock.get()){
                return false;
            }
            strongSelf->onSockSend();
			return true;
		});
        _sock->setOnRead([weakSelf,sock_ptr](const Buffer::Ptr &pBuf, struct sockaddr *addr) {
			auto strongSelf = weakSelf.lock();
			if (!strongSelf) {
				return;
			}
            if(sock_ptr != strongSelf->_sock.get()){
                return;
            }
            strongSelf->onSockRecv(pBuf);
		});
        _managerTimer = std::make_shared<Timer>(2,[weakSelf](){
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return false;
            }
            strongSelf->onManager();
            return true;
        },_executor);
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

} /* namespace toolkit */
