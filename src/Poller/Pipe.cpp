/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <fcntl.h>
#include "Pipe.h"
#include "Network/sockutil.h"

using namespace std;

namespace toolkit {

Pipe::Pipe(const onRead &cb, const EventPoller::Ptr &poller) {
    _poller = poller;
    if (!_poller) {
        _poller = EventPollerPool::Instance().getPoller();
    }
    _pipe = std::make_shared<PipeWrap>();
    auto pipe = _pipe;
    _poller->addEvent(_pipe->readFD(), EventPoller::Event_Read, [cb, pipe](int event) {
#if defined(_WIN32)
        unsigned long nread = 1024;
#else
        int nread = 1024;
#endif //defined(_WIN32)
        ioctl(pipe->readFD(), FIONREAD, &nread);
#if defined(_WIN32)
        std::shared_ptr<char> buf(new char[nread + 1], [](char *ptr) {delete[] ptr; });
        buf.get()[nread] = '\0';
        nread = pipe->read(buf.get(), nread + 1);
        if (cb) {
            cb(nread, buf.get());
        }
#else
        char buf[nread + 1];
        buf[nread] = '\0';
        nread = pipe->read(buf, sizeof(buf));
        if (cb) {
            cb(nread, buf);
        }
#endif // defined(_WIN32)
    });
}

Pipe::~Pipe() {
    if (_pipe) {
        auto pipe = _pipe;
        _poller->delEvent(pipe->readFD(), [pipe](bool success) {});
    }
}

void Pipe::send(const char *buf, int size) {
    _pipe->write(buf, size);
}

}  // namespace toolkit