/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xia-chu/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "TcpClient.h"

namespace toolkit {

StatisticImp(TcpClient);

TcpClient::TcpClient(const EventPoller::Ptr &poller) : SocketHelper(nullptr) {
    setPoller(poller ? poller : EventPollerPool::Instance().getPoller());
    setOnCreateSocket([](const EventPoller::Ptr &poller) {
        //TCP客户端默认开启互斥锁
        return Socket::createSocket(poller, true);
    });
}

TcpClient::~TcpClient() {}

void TcpClient::shutdown(const SockException &ex) {
    _timer.reset();
    SocketHelper::shutdown(ex);
}

bool TcpClient::alive() {
    auto sock = getSock();
    bool ret = sock && sock->rawFD() >= 0;
    return ret;
}

void TcpClient::setNetAdapter(const string &local_ip){
    _net_adapter = local_ip;
}

void TcpClient::startConnect(const string &url, uint16_t port, float timeout_sec) {
    _timer.reset();
    weak_ptr<TcpClient> weakSelf = shared_from_this();
    setSock(createSocket());
    getSock()->connect(url, port, [weakSelf](const SockException &err) {
        auto strongSelf = weakSelf.lock();
        if (strongSelf) {
            strongSelf->onSockConnect(err);
        }
    }, timeout_sec, _net_adapter.data());
}

void TcpClient::onSockConnect(const SockException &ex) {
    if (ex) {
        //连接失败
        _timer.reset();
        onConnect(ex);
        return;
    }

    auto sock_ptr = getSock().get();
    weak_ptr<TcpClient> weakSelf = shared_from_this();

    getSock()->setOnErr([weakSelf, sock_ptr](const SockException &ex) {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        if (sock_ptr != strongSelf->getSock().get()) {
            //已经重连socket，上次的socket的事件忽略掉
            return;
        }
        strongSelf->_timer.reset();
        strongSelf->onErr(ex);
    });

    getSock()->setOnFlush([weakSelf, sock_ptr]() {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return false;
        }
        if (sock_ptr != strongSelf->getSock().get()) {
            //已经重连socket，上传socket的事件忽略掉
            return false;
        }
        strongSelf->onFlush();
        return true;
    });

    getSock()->setOnRead([weakSelf, sock_ptr](const Buffer::Ptr &pBuf, struct sockaddr *, int) {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        if (sock_ptr != strongSelf->getSock().get()) {
            //已经重连socket，上传socket的事件忽略掉
            return;
        }
        strongSelf->onRecv(pBuf);
    });

    _timer = std::make_shared<Timer>(2.0f, [weakSelf]() {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return false;
        }
        strongSelf->onManager();
        return true;
    }, getPoller());

    onConnect(ex);
}

} /* namespace toolkit */
