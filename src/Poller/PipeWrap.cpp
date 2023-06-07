/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <stdexcept>
#include "PipeWrap.h"
#include "Util/util.h"
#include "Util/uv_errno.h"
#include "Network/sockutil.h"

using namespace std;

#define checkFD(fd) \
    if (fd == -1) { \
        clearFD(); \
        throw runtime_error(StrPrinter << "Create windows pipe failed: " << get_uv_errmsg());\
    }

#define closeFD(fd) \
    if (fd != -1) { \
        close(fd);\
        fd = -1;\
    }

namespace toolkit {

PipeWrap::PipeWrap() {
    reOpen();
}

void PipeWrap::reOpen() {
    clearFD();
#if defined(_WIN32)
    const char *localip = SockUtil::support_ipv6() ? "::1" : "127.0.0.1";
    auto listener_fd = SockUtil::listen(0, localip);
    checkFD(listener_fd)
    SockUtil::setNoBlocked(listener_fd,false);
    auto localPort = SockUtil::get_local_port(listener_fd);
    _pipe_fd[1] = SockUtil::connect(localip, localPort,false);
    checkFD(_pipe_fd[1])
    _pipe_fd[0] = (int)accept(listener_fd, nullptr, nullptr);
    checkFD(_pipe_fd[0])
    SockUtil::setNoDelay(_pipe_fd[0]);
    SockUtil::setNoDelay(_pipe_fd[1]);
    close(listener_fd);
#else
    if (pipe(_pipe_fd) == -1) {
        throw runtime_error(StrPrinter << "Create posix pipe failed: " << get_uv_errmsg());
    }
#endif // defined(_WIN32)
    SockUtil::setNoBlocked(_pipe_fd[0], true);
    SockUtil::setNoBlocked(_pipe_fd[1], false);
    SockUtil::setCloExec(_pipe_fd[0]);
    SockUtil::setCloExec(_pipe_fd[1]);
}

void PipeWrap::clearFD() {
    closeFD(_pipe_fd[0]);
    closeFD(_pipe_fd[1]);
}

PipeWrap::~PipeWrap() {
    clearFD();
}

int PipeWrap::write(const void *buf, int n) {
    int ret;
    do {
#if defined(_WIN32)
        ret = send(_pipe_fd[1], (char *)buf, n, 0);
#else
        ret = ::write(_pipe_fd[1], buf, n);
#endif // defined(_WIN32)
    } while (-1 == ret && UV_EINTR == get_uv_error(true));
    return ret;
}

int PipeWrap::read(void *buf, int n) {
    int ret;
    do {
#if defined(_WIN32)
        ret = recv(_pipe_fd[0], (char *)buf, n, 0);
#else
        ret = ::read(_pipe_fd[0], buf, n);
#endif // defined(_WIN32)
    } while (-1 == ret && UV_EINTR == get_uv_error(true));
    return ret;
}

} /* namespace toolkit*/
