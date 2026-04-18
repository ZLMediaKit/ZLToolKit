/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <csignal>
#include <iostream>
#include <string>

#include "Network/Socket.h"
#include "Thread/semaphore.h"
#include "Util/logger.h"
#include "Util/util.h"

using namespace std;
using namespace toolkit;

int main() {
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    auto recv_sock = Socket::createSocket();
    auto send_sock = Socket::createSocket();
    if (!recv_sock || !send_sock) {
        cerr << "create socket failed" << endl;
        return 1;
    }

    auto invalid_cfg_buffer = SocketRecvBuffer::create(true, 0, 1);
    if (!invalid_cfg_buffer) {
        cerr << "create invalid_cfg_buffer failed" << endl;
        return 2;
    }

    if (!recv_sock->setUdpRecvBuffer(invalid_cfg_buffer)) {
        cerr << "setUdpRecvBuffer should succeed before fd creation" << endl;
        return 3;
    }

    recv_sock->setIgnoreUdpConnRefused(true);

    if (!recv_sock->bindUdpSock(0, "127.0.0.1")) {
        cerr << "bind recv socket failed" << endl;
        return 4;
    }
    if (!send_sock->bindUdpSock(0, "127.0.0.1")) {
        cerr << "bind send socket failed" << endl;
        return 5;
    }

    if (recv_sock->setUdpRecvBuffer(SocketRecvBuffer::create(true, 1, 4096))) {
        cerr << "setUdpRecvBuffer should fail after fd creation" << endl;
        return 6;
    }

    semaphore sem;
    string received;
    recv_sock->setOnRead([&](const Buffer::Ptr &buf, struct sockaddr *, int) {
        received.assign(buf ? buf->data() : "", buf ? buf->size() : 0);
        sem.post();
    });

    auto dst = SockUtil::make_sockaddr("127.0.0.1", recv_sock->get_local_port());
    const string payload = "udp-buffer-config-smoke";
    if (send_sock->send(payload, reinterpret_cast<struct sockaddr *>(&dst)) <= 0) {
        cerr << "send payload failed" << endl;
        return 7;
    }

    if (!sem.wait(3000)) {
        cerr << "recv timeout" << endl;
        return 8;
    }

    if (received != payload) {
        cerr << "unexpected payload: " << received << endl;
        return 9;
    }

    cout << "udp socket buffer config regression passed" << endl;
    return 0;
}
