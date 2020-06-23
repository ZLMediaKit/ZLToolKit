/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "TcpClient.h"

namespace toolkit {

TcpClient::TcpClient(const EventPoller::Ptr &poller) : SocketHelper(nullptr) {
    _poller = poller;
    if(!_poller){
        _poller = EventPollerPool::Instance().getPoller();
    }
}

TcpClient::~TcpClient() {}

void TcpClient::shutdown(const SockException &ex) {
    _managerTimer.reset();
    SocketHelper::shutdown(ex);
}

bool TcpClient::alive() {
    bool ret = _sock.operator bool() && _sock->rawFD() >= 0;
    return ret;
}

void TcpClient::setNetAdapter(const string &localIp){
    _netAdapter = localIp;
}

void TcpClient::startConnect(const string &strUrl, uint16_t iPort,float fTimeOutSec) {
    weak_ptr<TcpClient> weakSelf = shared_from_this();
    _managerTimer.reset();
    _sock = std::make_shared<Socket>(_poller);
    _sock->connect(strUrl, iPort, [weakSelf](const SockException &err){
        auto strongSelf = weakSelf.lock();
        if(strongSelf){
            strongSelf->onSockConnect(err);
        }
    }, fTimeOutSec,_netAdapter.data());
}
void TcpClient::onSockConnect(const SockException &ex) {
    if(ex){
        //连接失败
        _managerTimer.reset();
        onConnect(ex);
        return;
    }

    weak_ptr<TcpClient> weakSelf = shared_from_this();
    auto sock_ptr = _sock.get();
    _sock->setOnErr([weakSelf,sock_ptr](const SockException &ex) {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        if(sock_ptr != strongSelf->_sock.get()){
            //已经重连socket，上传socket的事件忽略掉
            return;
        }
        strongSelf->_managerTimer.reset();
        strongSelf->onErr(ex);
    });
    _sock->setOnFlush([weakSelf,sock_ptr]() {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return false;
        }
        if(sock_ptr != strongSelf->_sock.get()){
            //已经重连socket，上传socket的事件忽略掉
            return false;
        }
        strongSelf->onFlush();
        return true;
    });
    _sock->setOnRead([weakSelf,sock_ptr](const Buffer::Ptr &pBuf, struct sockaddr * , int) {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        if(sock_ptr != strongSelf->_sock.get()){
            //已经重连socket，上传socket的事件忽略掉
            return;
        }
        strongSelf->onRecv(pBuf);
    });
    _managerTimer = std::make_shared<Timer>(2,[weakSelf](){
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return false;
        }
        strongSelf->onManager();
        return true;
    },_poller);

    onConnect(ex);
}

string TcpClient::getIdentifier() const{
    return  to_string(reinterpret_cast<uint64_t>(this));
}

} /* namespace toolkit */
