/*
 * Copyright (c) 2021 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <atomic>
#include "Session.h"

using namespace std;

namespace toolkit {

class TcpSession : public Session {};
class UdpSession : public Session {};

StatisticImp(UdpSession)
StatisticImp(TcpSession)

Session::Session(const Socket::Ptr &sock) : SocketHelper(sock) {
    if (sock->sockType() == SockNum::Sock_TCP) {
        _statistic_tcp.reset(new ObjectStatistic<TcpSession>);
    } else {
        _statistic_udp.reset(new ObjectStatistic<UdpSession>);
    }
}

string Session::getIdentifier() const {
    if (_id.empty()) {
        static atomic<uint64_t> s_session_index{0};
        _id = to_string(++s_session_index) + '-' + to_string(getSock()->rawFD());
    }
    return _id;
}

} // namespace toolkit
