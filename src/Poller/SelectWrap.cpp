/*
 * WinSelect.cpp
 *
 *  Created on: 2017年3月2日
 *      Author: Jzan
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


