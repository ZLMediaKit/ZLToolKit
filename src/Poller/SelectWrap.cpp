/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
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

namespace toolkit {

FdSet::FdSet() {
    _ptr = new fd_set;
}

FdSet::~FdSet() {
    delete (fd_set *)_ptr;
}

void FdSet::fdZero() {
    FD_ZERO((fd_set *)_ptr);
}

void FdSet::fdClr(int fd) {
    FD_CLR(fd, (fd_set *)_ptr);
}

void FdSet::fdSet(int fd) {
    FD_SET(fd, (fd_set *)_ptr);
}

bool FdSet::isSet(int fd) {
    return  FD_ISSET(fd, (fd_set *)_ptr);
}

int zl_select(int cnt, FdSet *read, FdSet *write, FdSet *err, struct timeval *tv) {
    void *rd, *wt, *er;
    rd = read ? read->_ptr : NULL;
    wt = write ? write->_ptr : NULL;
    er = err ? err->_ptr : NULL;
    return ::select(cnt, (fd_set *) rd, (fd_set *) wt, (fd_set *) er, tv);
}


} /* namespace toolkit */



