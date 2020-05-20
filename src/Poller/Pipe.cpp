/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <fcntl.h>
#include "Pipe.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Network/sockutil.h"

using namespace std;

namespace toolkit {

Pipe::Pipe(const function<void(int size, const char *buf)> &onRead,
           const EventPoller::Ptr &poller) {
    _poller = poller;
    if(!_poller){
        _poller =  EventPollerPool::Instance().getPoller();
    }
    _pipe = std::make_shared<PipeWrap>();
    auto pipeCopy = _pipe;
    _poller->addEvent(_pipe->readFD(), Event_Read, [onRead, pipeCopy](int event) {
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
        _poller->delEvent(pipeCopy->readFD(), [pipeCopy](bool success) {});
    }
}
void Pipe::send(const char *buf, int size) {
    _pipe->write(buf,size);
}


}  // namespace toolkit

