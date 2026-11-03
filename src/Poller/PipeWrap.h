/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PipeWarp_h
#define PipeWarp_h

#include <string>

namespace toolkit {

class PipeWrap {
public:
    PipeWrap();
    ~PipeWrap();
    int write(const void *buf, int n);
    int read(void *buf, int n);
    int readFD() const { return _pipe_fd[0]; }
    int writeFD() const { return _pipe_fd[1]; }
    void reOpen();

private:
    void clearFD();
#if defined(_WIN32)
    // AF_UNIX模拟管道，成功返回true；系统不支持时返回false
    bool reOpenUnix();
    // 回环TCP模拟管道，作为AF_UNIX不可用时的降级方案
    void reOpenTcp();
#endif // defined(_WIN32)

private:
    int _pipe_fd[2] = { -1, -1 };
};

} /* namespace toolkit */
#endif // !PipeWarp_h

