//
//  Pipe.cpp
//  xzl
//
//  Created by xzl on 16/4/13.
//

#include <fcntl.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <Poller/Pipe.h>
#include <unistd.h>
#include "Util/logger.h"
#include "Util/util.h"
#include "Network/sockutil.h"

using namespace std;
using namespace ZL::Util;
using ZL::Network::SockUtil;

namespace ZL {
namespace Poller {
Pipe::Pipe(function<void(int size, const char *buf)> &&onRead) {
	if (pipe(pipe_fd)) {
		throw runtime_error(StrPrinter << "创建管道失败：" << errno << endl);
	}
	SockUtil::setNoBlocked(pipe_fd[0]);
	SockUtil::setNoBlocked(pipe_fd[1]);
	if (!onRead) {
		onRead = [](int size,const char *buf) {
			TraceL<<"收到["<<size<<"]:"<<buf;
		};
	}
	int fd = pipe_fd[0];
	EventPoller::Instance().addEvent(pipe_fd[0], Event_Read,
			[onRead,fd](int event) {
				int nread;
				ioctl(fd, FIONREAD, &nread);
				char buf[nread+1];
				buf[nread]='\0';

				nread=(int)read(fd, buf, sizeof(buf));
				onRead(nread,buf);
			});
}
Pipe::~Pipe() {

	if (pipe_fd[0]) {
		int fd = pipe_fd[0];
		EventPoller::Instance().delEvent(pipe_fd[0], [fd](bool success) {
			close(fd);
		});
		pipe_fd[0] = -1;
	}
	if (pipe_fd[1]) {
		int fd = pipe_fd[1];
		EventPoller::Instance().delEvent(pipe_fd[1], [fd](bool success) {
			close(fd);
		});
		pipe_fd[1] = -1;
	}

}
void Pipe::send(const char *buf, int size) {
	if (size) {
		writeBuf.append(buf, size);
	} else {
		writeBuf.append(buf);
	}
	if (canWrite) {
		canWrite = false;
		EventPoller::Instance().addEvent(pipe_fd[1], Event_Write,
				bind(&Pipe::willWrite, this, pipe_fd[1], placeholders::_1));
		willWrite(pipe_fd[1], Event_Write);
	}
}

void Pipe::willWrite(int fd, int event) {
	if (writeBuf.size()) {
		write(pipe_fd[1], writeBuf.c_str(), writeBuf.size() + 1);
		writeBuf.clear();
	} else {
		canWrite = true;
		EventPoller::Instance().delEvent(fd);
	}
}

}  // namespace Poller
}  // namespace ZL

