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
	SockUtil::setNoBlocked(pipe_fd[1],false);
	int fd = pipe_fd[0];
	EventPoller::Instance().addEvent(pipe_fd[0], Event_Read, [onRead,fd](int event) {
		int nread;
		ioctl(fd, FIONREAD, &nread);
		char buf[nread+1];
		buf[nread]='\0';
		nread=(int)read(fd, buf, sizeof(buf));
		if(onRead){
			onRead(nread,buf);
		}
	});
}
Pipe::~Pipe() {
	if (pipe_fd[0] > 0) {
		int fd = pipe_fd[0];
		EventPoller::Instance().delEvent(pipe_fd[0], [fd](bool success) {
			close(fd);
		});
		pipe_fd[0] = -1;
	}
	if (pipe_fd[1] > 0) {
		close(pipe_fd[1]);
		pipe_fd[1] = -1;
	}
}
void Pipe::send(const char *buf, int size) {
	write(pipe_fd[1], buf,size);
}


}  // namespace Poller
}  // namespace ZL

