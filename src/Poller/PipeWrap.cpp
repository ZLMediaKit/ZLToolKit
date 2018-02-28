/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <stdexcept>
#include "PipeWrap.h"
#include "Util/util.h"
#include "Util/uv_errno.h"
#include "Network/sockutil.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Network;

#define checkFD(fd) \
	if (fd == -1) { \
		clearFD(); \
		throw runtime_error(StrPrinter << "create windows pipe failed:" << get_uv_errmsg());\
	}

#define closeFD(fd) \
	if (fd != -1) { \
		close(fd);\
		fd = -1;\
	}

namespace ZL {
namespace Poller {

PipeWrap::PipeWrap(){

#if defined(_WIN32)
	_listenerFd = SockUtil::listen(0, "127.0.0.1");
	checkFD(_listenerFd)
	SockUtil::setNoBlocked(_listenerFd,false);
	auto localPort = SockUtil::get_local_port(_listenerFd);
	_pipe_fd[1] = SockUtil::connect("127.0.0.1", localPort,false);
	checkFD(_pipe_fd[1])
	_pipe_fd[0] = accept(_listenerFd, nullptr, nullptr);
	checkFD(_pipe_fd[0])

#else
	if (pipe(_pipe_fd) == -1) {
		throw runtime_error(StrPrinter << "create posix pipe failed:" << get_uv_errmsg());\
	}
#endif // defined(_WIN32)	
	SockUtil::setNoBlocked(_pipe_fd[0],true);
	SockUtil::setNoBlocked(_pipe_fd[1],false);
}

void PipeWrap::clearFD() {
	closeFD(_pipe_fd[0]);
	closeFD(_pipe_fd[1]);

#if defined(_WIN32)
	closeFD(_listenerFd);
#endif // defined(_WIN32)

}
PipeWrap::~PipeWrap(){
	clearFD();
}

int PipeWrap::write(const void *buf, int n) {
	int ret;
	do {
#if defined(_WIN32)
		ret = send(_pipe_fd[1], (char *)buf, n, 0);
#else
		ret = ::write(_pipe_fd[1],buf,n);
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

} /* namespace Poller */
} /* namespace ZL*/
