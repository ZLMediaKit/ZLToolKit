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

Session::Session(const Socket::Ptr &sock) : SocketHelper(sock) {}
Session::~Session() = default;

static atomic<uint64_t> s_session_index{0};

string Session::getIdentifier() const {
    if (_id.empty()) {
        _id = to_string(++s_session_index) + '-' + to_string(getSock()->rawFD());
    }
    return _id;
}

void Session::safeShutdown(const SockException &ex) {
    std::weak_ptr<Session> weakSelf = shared_from_this();
    async_first([weakSelf,ex](){
        auto strongSelf = weakSelf.lock();
        if (strongSelf) {
            strongSelf->shutdown(ex);
        }
    });
}

StatisticImp(Session)
StatisticImp(UdpSession)
StatisticImp(TcpSession)

} // namespace toolkit
