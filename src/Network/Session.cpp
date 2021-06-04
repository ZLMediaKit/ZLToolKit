/*
 * Copyright (c) 2021 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Network/Session.h"

namespace toolkit {

Session::Session(const Socket::Ptr &sock) : SocketHelper(sock) {
}

Session::~Session() = default;

string Session::getIdentifier() const{
    return std::to_string(reinterpret_cast<uint64_t>(this));
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
