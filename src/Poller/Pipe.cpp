//
//  Pipe.cpp
//  xzl
//
//  Created by xzl on 16/4/13.
//

#include <fcntl.h>
#include "Pipe.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Network/sockutil.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Network;

namespace ZL {
namespace Poller {
Pipe::Pipe(function<void(int size, const char *buf)> &&onRead) {
	_pipe.reset(new PipeWrap);
	auto pipeCopy = _pipe;
	EventPoller::Instance().addEvent(_pipe->readFD(), Event_Read, [onRead, pipeCopy](int event) {
#if defined(_WIN32)
		unsigned long nread = 1024;
#else
		int nread = 1024;
#endif //defined(_WIN32)
		ioctl(pipeCopy->readFD(), FIONREAD, &nread);
#if defined(_WIN32)
		std::shared_ptr<char> buf(new char[nread + 1], [](char *ptr) {delete[] ptr; });
		buf.get()[nread] = '\0';
		nread = pipeCopy->read(buf.get(), nread + 1);
		if (onRead) {
			onRead(nread, buf.get());
		}
#else
		char buf[nread + 1];
		buf[nread] = '\0';
		nread = pipeCopy->read(buf, sizeof(buf));
		if (onRead) {
			onRead(nread, buf);
		}
#endif // defined(_WIN32)
		
	});
}
Pipe::~Pipe() {
	if (_pipe) {
		auto pipeCopy = _pipe;
		EventPoller::Instance().delEvent(pipeCopy->readFD(), [pipeCopy](bool success) {});
	}
}
void Pipe::send(const char *buf, int size) {
	_pipe->write(buf,size);
}


}  // namespace Poller
}  // namespace ZL

