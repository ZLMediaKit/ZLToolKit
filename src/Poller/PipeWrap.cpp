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
#include "Util/logger.h"
#include "Util/uv_errno.h"
#include "Network/sockutil.h"

#if defined(_WIN32)
#include <windows.h>
#include <mutex>
// Windows select 要求同一次调用中的 socket 来自相同 provider，
// 因此 AF_UNIX 管道仅用于 wepoll，select 后端使用回环 TCP。
#if defined(HAS_EPOLL) && defined(__has_include)
#if __has_include(<afunix.h>)
#define PIPE_HAS_AF_UNIX 1
#include <afunix.h>
#include <atomic>
#endif
#endif
#endif

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
#if defined(PIPE_HAS_AF_UNIX)
    bool unix_mode = reOpenUnix();
    if (!unix_mode) {
        // AF_UNIX不可用(低于Win10 1803或运行时创建失败)，降级为回环TCP模拟
        reOpenTcp();
    }
    const char *pipe_mode = unix_mode ? "AF_UNIX" : "loopback TCP (fallback)";
#elif !defined(HAS_EPOLL)
    reOpenTcp();
    const char *pipe_mode = "loopback TCP (AF_UNIX requires wepoll)";
#else
    reOpenTcp();
    const char *pipe_mode = "loopback TCP (AF_UNIX SDK header unavailable)";
#endif
    static std::once_flag log_pipe_mode_once;
    std::call_once(log_pipe_mode_once, [pipe_mode]() {
        InfoL << "PipeWrap created, mode: " << pipe_mode;
    });
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

#if defined(_WIN32)
void PipeWrap::reOpenTcp() {
    const char *localip = "127.0.0.1";
    int listener_fd = -1;
    onceToken listener_guard(nullptr, [&]() { closeFD(listener_fd); });
    listener_fd = SockUtil::listen(0, localip);
    checkFD(listener_fd)
    SockUtil::setNoBlocked(listener_fd,false);
    auto localPort = SockUtil::get_local_port(listener_fd);
    _pipe_fd[1] = SockUtil::connect(localip, localPort,false);
    checkFD(_pipe_fd[1])
    _pipe_fd[0] = (int)accept(listener_fd, nullptr, nullptr);
    checkFD(_pipe_fd[0])
    SockUtil::setNoDelay(_pipe_fd[0]);
    SockUtil::setNoDelay(_pipe_fd[1]);
}
#endif // defined(_WIN32)

#if defined(_WIN32) && defined(PIPE_HAS_AF_UNIX)
static std::wstring makeUnixPipePath() {
    static atomic<uint32_t> s_counter { 0 };
    wchar_t temp[MAX_PATH];
    auto len = GetTempPathW(MAX_PATH, temp);
    if (len == 0 || len >= MAX_PATH) {
        return {};
    }
    std::wstring path(temp, len);
    if (path.back() != L'\\' && path.back() != L'/') {
        path += L'\\';
    }
    path += L"zltk_pipe_" + std::to_wstring(GetCurrentProcessId())
        + L"_" + std::to_wstring(s_counter.fetch_add(1, std::memory_order_relaxed))
        + L".sock";
    return path;
}

bool PipeWrap::reOpenUnix() {
    auto wsPath = makeUnixPipePath();
    if (wsPath.empty()) {
        WarnL << "Failed to generate AF_UNIX socket path, fallback to loopback TCP";
        return false;
    }

    sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    if (WideCharToMultiByte(CP_UTF8, 0, wsPath.c_str(), -1,
                            addr.sun_path, static_cast<int>(sizeof(addr.sun_path)),
                            nullptr, nullptr) == 0) {
        WarnL << "WideCharToMultiByte failed: " << GetLastError() << ", fallback to loopback TCP";
        return false;
    }
    int listener_fd = -1;
    bool bound = false;
    bool success = false;
    onceToken cleanup(nullptr, [&]() {
        closeFD(listener_fd);
        if (!success) {
            clearFD();
        }
        // 仅删除本次成功绑定的路径，不能删除其他进程的文件。
        if (bound && !DeleteFileW(wsPath.c_str())) {
            auto error = GetLastError();
            WarnL << "Delete AF_UNIX socket file " << addr.sun_path <<  " failed: " << error;
        }
    });

    listener_fd = (int)socket(AF_UNIX, SOCK_STREAM, 0);
    if (listener_fd == -1) {
        DebugL << "AF_UNIX socket() failed: " << get_uv_errmsg(true) << ", fallback to loopback TCP";
        return false;
    }

    if (::bind(listener_fd, (sockaddr *)&addr, sizeof(addr)) == -1) {
        DebugL << "AF_UNIX bind() failed: " << get_uv_errmsg(true) << ", fallback to loopback TCP";
        return false;
    }
    bound = true;
    if (listen(listener_fd, 1) == -1) {
        DebugL << "AF_UNIX listen() failed: " << get_uv_errmsg(true) << ", fallback to loopback TCP";
        return false;
    }
    _pipe_fd[1] = (int)socket(AF_UNIX, SOCK_STREAM, 0);
    if (_pipe_fd[1] == -1) {
        DebugL << "AF_UNIX client socket() failed: " << get_uv_errmsg(true) << ", fallback to loopback TCP";
        return false;
    }
    if (connect(_pipe_fd[1], (sockaddr *)&addr, sizeof(addr)) == -1) {
        DebugL << "AF_UNIX connect() failed: " << get_uv_errmsg(true) << ", fallback to loopback TCP";
        return false;
    }
    _pipe_fd[0] = (int)accept(listener_fd, nullptr, nullptr);
    if (_pipe_fd[0] == -1) {
        DebugL << "AF_UNIX accept() failed: " << get_uv_errmsg(true) << ", fallback to loopback TCP";
        return false;
    }

    success = true;
    return success;
}
#endif // defined(_WIN32) && defined(PIPE_HAS_AF_UNIX)

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
