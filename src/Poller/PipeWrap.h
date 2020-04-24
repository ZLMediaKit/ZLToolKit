/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PipeWarp_h
#define PipeWarp_h

namespace toolkit {

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

} /* namespace toolkit */
#endif // !PipeWarp_h

