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
Pipe::Pipe(const function<void(int size, const char *buf)> &onRead) {
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

