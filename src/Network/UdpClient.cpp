/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "UdpClient.h"

using namespace std;

namespace toolkit {

StatisticImp(UdpClient)

UdpClient::UdpClient(const EventPoller::Ptr &poller) : SocketHelper(nullptr) {
    setPoller(poller ? poller : EventPollerPool::Instance().getPoller());
    setOnCreateSocket([](const EventPoller::Ptr &poller) {
        //TCP客户端默认开启互斥锁
        return Socket::createSocket(poller, true);
    });
}

UdpClient::~UdpClient() {
    TraceL << "~" << UdpClient::getIdentifier();
}

void UdpClient::startConnect(const string &peer_host, uint16_t peer_port, uint16_t local_port) {
    weak_ptr<UdpClient> weak_self = static_pointer_cast<UdpClient>(shared_from_this());
    _timer = std::make_shared<Timer>(2.0f, [weak_self]() {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return false;
        }
        strong_self->onManager();
        return true;
    }, getPoller());

    setSock(createSocket());

    auto sock_ptr = getSock().get();
    sock_ptr->setOnErr([weak_self, sock_ptr](const SockException &ex) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        if (sock_ptr != strong_self->getSock().get()) {
            //已经重连socket，上次的socket的事件忽略掉
            return;
        }
        strong_self->_timer.reset();
        TraceL << strong_self->getIdentifier() << " on err: " << ex;
        strong_self->onError(ex);
    });

    sock_ptr->setOnFlush([weak_self, sock_ptr]() {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return false;
        }
        if (sock_ptr != strong_self->getSock().get()) {
            //已经重连socket，上传socket的事件忽略掉
            return false;
        }
        strong_self->onFlush();
        return true;
    });

    sock_ptr->setOnRead([weak_self, sock_ptr](const Buffer::Ptr &pBuf, struct sockaddr * addr, int addr_len) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        if (sock_ptr != strong_self->getSock().get()) {
            //已经重连socket，上传socket的事件忽略掉
            return;
        }
        try {
            strong_self->onRecvFrom(pBuf, addr, addr_len);
        } catch (std::exception &ex) {
            strong_self->shutdown(SockException(Err_other, ex.what()));
        }
    });

    bool ret = getSock()->bindUdpSock(local_port, _net_adapter);
    if (!ret) {
        WarnL << "UDP output bind local error";
    }
    auto peer_addr = SockUtil::make_sockaddr(peer_host.c_str(), peer_port);

    //只能软绑定
    ret = getSock()->bindPeerAddr((struct sockaddr *)&peer_addr, 0, true);
    if (!ret) {
        WarnL << "UDP output bind peer error";
    }
    // TraceL << getIdentifier() << " start connect " << url << ":" << peer_port;
}

void UdpClient::shutdown(const SockException &ex) {
    _timer.reset();
    SocketHelper::shutdown(ex);
}

bool UdpClient::alive() const {
    if (_timer) {
        //连接中或已连接
        return true;
    }
    //在websocket client(zlmediakit)相关代码中，
    //_timer一直为空，但是socket fd有效，alive状态也应该返回true
    auto sock = getSock();
    return sock && sock->alive();
}

void UdpClient::setNetAdapter(const string &local_ip) {
    _net_adapter = local_ip;
}

std::string UdpClient::getIdentifier() const {
    if (_id.empty()) {
        static atomic<uint64_t> s_index { 0 };
        _id = toolkit::demangle(typeid(*this).name()) + "-" + to_string(++s_index);
    }
    return _id;
}

} /* namespace toolkit */
