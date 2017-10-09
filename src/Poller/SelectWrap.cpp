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

#include <iostream>
#include <sys/types.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <string.h>
#include "SelectWrap.h"
#include "Util/util.h"
#include "Util/uv_errno.h"

using namespace std;
using namespace ZL::Util;

namespace ZL {
namespace Poller {


FdSet::FdSet() {
	ptr = new fd_set;
}

FdSet::~FdSet() {
	delete (fd_set *)ptr;
}

void FdSet::fdZero() {
	FD_ZERO((fd_set *)ptr);
}

void FdSet::fdClr(int fd) {
	FD_CLR(fd, (fd_set *)ptr);
}

void FdSet::fdSet(int fd) {
	FD_SET(fd, (fd_set *)ptr);
}

bool FdSet::isSet(int fd) {
	return  FD_ISSET(fd, (fd_set *)ptr);
}

} /* namespace Poller */
} /* namespace ZL */

int zl_select(int cnt,FdSet *read,FdSet *write,FdSet *err,struct timeval *tv){
	void *rd,*wt,*er;
	rd = read?read->ptr:NULL;
	wt = write?write->ptr:NULL;
	er = err?err->ptr:NULL;
	return ::select(cnt,(fd_set*)rd,(fd_set*)wt,(fd_set*)er,tv);
}


