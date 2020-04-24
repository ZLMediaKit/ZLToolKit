/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <string>
#include "TcpSession.h"

namespace toolkit {

TcpSession::TcpSession( const Socket::Ptr &sock) : SocketHelper(sock) {
}

TcpSession::~TcpSession() {
}

string TcpSession::getIdentifier() const{
    return  to_string(reinterpret_cast<uint64_t>(this));
}

void TcpSession::safeShutdown(const SockException &ex){
    std::weak_ptr<TcpSession> weakSelf = shared_from_this();
    async_first([weakSelf,ex](){
        auto strongSelf = weakSelf.lock();
        if(strongSelf){
            strongSelf->shutdown(ex);
        }
    });
}

} /* namespace toolkit */

