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
#ifndef PipeWarp_h
#define PipeWarp_h

namespace ZL {
namespace Poller {

class PipeWrap {
public:
	PipeWrap();
	~PipeWrap();
	int write(const void *buf, int n);
	int read(void *buf, int n);
	int readFD() const {
		return _pipe_fd[0];
	}
	int writeFD() const {
		return _pipe_fd[1];
	}
private:
	int _pipe_fd[2] = { -1,-1 };
	void clearFD();
#if defined(_WIN32)
	int _listenerFd = -1;
#endif // defined(_WIN32)

		};

} /* namespace Poller */
} /* namespace ZL */

#endif // !PipeWarp_h

