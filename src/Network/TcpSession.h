/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SERVER_SESSION_H_
#define SERVER_SESSION_H_

#include "Session.h"
#include "Util/SSLBox.h"

namespace toolkit {

//通过该模板可以让TCP服务器快速支持TLS
template<typename TcpSessionType>
class TcpSessionWithSSL : public TcpSessionType {
public:
    template<typename ...ArgsType>
    TcpSessionWithSSL(ArgsType &&...args):TcpSessionType(std::forward<ArgsType>(args)...) {
        _ssl_box.setOnEncData([&](const Buffer::Ptr &buf) {
            public_send(buf);
        });
        _ssl_box.setOnDecData([&](const Buffer::Ptr &buf) {
            public_onRecv(buf);
        });
    }

    ~TcpSessionWithSSL() override {
        _ssl_box.flush();
    }

    void onRecv(const Buffer::Ptr &buf) override {
        _ssl_box.onRecv(buf);
    }

    //添加public_onRecv和public_send函数是解决较低版本gcc一个lambad中不能访问protected或private方法的bug
    inline void public_onRecv(const Buffer::Ptr &buf) {
        TcpSessionType::onRecv(buf);
    }

    inline void public_send(const Buffer::Ptr &buf) {
        TcpSessionType::send(std::move(const_cast<Buffer::Ptr &>(buf)));
    }

protected:
    ssize_t send(Buffer::Ptr buf) override {
        auto size = buf->size();
        _ssl_box.onSend(std::move(buf));
        return size;
    }

private:
    SSL_Box _ssl_box;
};

} /* namespace toolkit */

#endif /* SERVER_SESSION_H_ */
