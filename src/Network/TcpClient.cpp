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
#include "TcpClient.h"

namespace toolkit {

TcpClient::TcpClient(const EventPoller::Ptr &poller) : SocketHelper(nullptr) {
    _poller = poller;
    if(!_poller){
        _poller = EventPollerPool::Instance().getPoller();
    }
    setPoller(_poller);
}

TcpClient::~TcpClient() {}

void TcpClient::shutdown() {
    _managerTimer.reset();
    if(_sock){
        _sock->closeSock();
    }
}

bool TcpClient::alive() {
    bool ret = _sock.operator bool() && _sock->rawFD() >= 0;
    return ret;
}

void TcpClient::setNetAdapter(const string &localIp){
    _netAdapter = localIp;
}

void TcpClient::startConnect(const string &strUrl, uint16_t iPort,float fTimeOutSec) {
    TcpClient::shutdown();
    SocketHelper::setSock(std::make_shared<Socket>(_poller));

    weak_ptr<TcpClient> weakSelf = shared_from_this();
    _sock->connect(strUrl, iPort, [weakSelf](const SockException &err){
        auto strongSelf = weakSelf.lock();
        if(strongSelf){
            strongSelf->onSockConnect(err);
        }
    }, fTimeOutSec,_netAdapter.data());
}
void TcpClient::onSockConnect(const SockException &ex) {
    if(ex){
        TcpClient::shutdown();
        return;
    }

    if(_sock){
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
        _managerTimer = std::make_shared<Timer>(2,[weakSelf](){
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return false;
            }
            strongSelf->onManager();
            return true;
        },_poller);
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
    TcpClient::shutdown();
    onErr(ex);
}

} /* namespace toolkit */
