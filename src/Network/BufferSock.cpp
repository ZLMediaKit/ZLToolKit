/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "BufferSock.h"
#include "Util/logger.h"
#include "Util/uv_errno.h"

namespace toolkit {

StatisticImp(BufferList)

/////////////////////////////////////// BufferSendMsg ///////////////////////////////////////

#if !defined(_WIN32)

class BufferSendMsg : public BufferList {
public:
    BufferSendMsg(List<std::pair<Buffer::Ptr, bool> > list, SendResult cb = nullptr);
    ~BufferSendMsg() override;

    bool empty() override;
    size_t count() override;
    ssize_t send(int fd, int flags) override;

private:
    void reOffset(size_t n);
    ssize_t send_l(int fd, int flags);

private:
    size_t _iovec_off = 0;
    size_t _remain_size = 0;
    SendResult _cb;
    std::vector<struct iovec> _iovec;
    List<std::pair<Buffer::Ptr, bool> > _pkt_list;
};

bool BufferSendMsg::empty() {
    return _iovec_off == _iovec.size();
}

size_t BufferSendMsg::count() {
    return _iovec.size() - _iovec_off;
}

ssize_t BufferSendMsg::send_l(int fd, int flags) {
    ssize_t n;
    do {
        struct msghdr msg;
        msg.msg_name = nullptr;
        msg.msg_namelen = 0;
        msg.msg_iov = &(_iovec[_iovec_off]);
        msg.msg_iovlen = (decltype(msg.msg_iovlen)) (_iovec.size() - _iovec_off);
        if (msg.msg_iovlen > IOV_MAX) {
            msg.msg_iovlen = IOV_MAX;
        }
        msg.msg_control = nullptr;
        msg.msg_controllen = 0;
        msg.msg_flags = flags;
        n = sendmsg(fd, &msg, flags);
    } while (-1 == n && UV_EINTR == get_uv_error(true));

    if (n >= (ssize_t) _remain_size) {
        //全部写完了
        _iovec_off = _iovec.size();
        _remain_size = 0;
        if (!_cb) {
            _pkt_list.clear();
            return n;
        }

        //全部发送成功回调
        while (!_pkt_list.empty()) {
            _cb(_pkt_list.front().first, true);
            _pkt_list.pop_front();
        }
        return n;
    }

    if (n > 0) {
        //部分发送成功
        reOffset(n);
        return n;
    }

    //一个字节都未发送
    return n;
}

ssize_t BufferSendMsg::send(int fd, int flags) {
    auto remain_size = _remain_size;
    while (_remain_size && send_l(fd, flags) != -1);

    ssize_t sent = remain_size - _remain_size;
    if (sent > 0) {
        //部分或全部发送成功
        return sent;
    }
    //一个字节都未发送成功
    return -1;
}

void BufferSendMsg::reOffset(size_t n) {
    _remain_size -= n;
    size_t offset = 0;
    auto last_off = _iovec_off;
    for (auto i = _iovec_off; i != _iovec.size(); ++i) {
        auto &ref = _iovec[i];
        offset += ref.iov_len;
        if (offset < n) {
            continue;
        }
        ssize_t remain = offset - n;
        ref.iov_base = (char *) ref.iov_base + ref.iov_len - remain;
        ref.iov_len = (decltype(ref.iov_len)) remain;
        _iovec_off = i;
        if (remain == 0) {
            _iovec_off += 1;
        }
        break;
    }

    //删除已经发送的数据，节省内存
    for (auto i = last_off; i < _iovec_off; ++i) {
        if (_cb) {
            //发送成功回调
            _cb(_pkt_list.front().first, true);
        }
        _pkt_list.pop_front();
    }
}

BufferSendMsg::BufferSendMsg(List<std::pair<Buffer::Ptr, bool>> list, SendResult cb)
    : _cb(std::move(cb))
    , _iovec(list.size())
    , _pkt_list(std::move(list)) {
    auto it = _iovec.begin();
    _pkt_list.for_each([&](std::pair<Buffer::Ptr, bool> &pr) {
        it->iov_base = pr.first->data();
        it->iov_len = (decltype(it->iov_len)) pr.first->size();
        _remain_size += it->iov_len;
        ++it;
    });
}

BufferSendMsg::~BufferSendMsg() {
    if (!_cb) {
        return;
    }
    //未发送成功的buffer
    while (!_pkt_list.empty()) {
        _cb(_pkt_list.front().first, false);
        _pkt_list.pop_front();
    }
}

BufferSock::BufferSock(Buffer::Ptr buffer, struct sockaddr *addr, int addr_len) {
    if (addr && addr_len) {
        _addr = (struct sockaddr *) malloc(addr_len);
        memcpy(_addr, addr, addr_len);
        _addr_len = addr_len;
    }
    assert(buffer);
    _buffer = std::move(buffer);
}

BufferSock::~BufferSock() {
    if (_addr) {
        free(_addr);
        _addr = nullptr;
    }
}

char *BufferSock::data() const {
    return _buffer->data();
}

size_t BufferSock::size() const {
    return _buffer->size();
}

const struct sockaddr *BufferSock::sockaddr() const {
    return _addr;
}

socklen_t BufferSock::socklen() const {
    return _addr_len;
}

#endif //!_WIN32

/////////////////////////////////////// BufferSendTo ///////////////////////////////////////

class BufferSendTo : public BufferList {
public:
    BufferSendTo(List<std::pair<Buffer::Ptr, bool> > list, SendResult cb, bool is_udp);
    ~BufferSendTo() override;

    bool empty() override;
    size_t count() override;
    ssize_t send(int fd, int flags) override;

private:
    bool _is_udp;
    SendResult _cb;
    size_t _offset = 0;
    List<std::pair<Buffer::Ptr, bool> > _pkt_list;
};

BufferSendTo::BufferSendTo(List<std::pair<Buffer::Ptr, bool>> list, BufferList::SendResult cb, bool is_udp)
    : _is_udp(is_udp)
    , _cb(std::move(cb))
    , _pkt_list(std::move(list)) {}

BufferSendTo::~BufferSendTo() {
    if (!_cb) {
        return;
    }
    //未发送成功的buffer
    while (!_pkt_list.empty()) {
        _cb(_pkt_list.front().first, false);
        _pkt_list.pop_front();
    }
}

bool BufferSendTo::empty() {
    return _pkt_list.empty();
}

size_t BufferSendTo::count() {
    return _pkt_list.size();
}

static inline BufferSock *getBufferSockPtr(std::pair<Buffer::Ptr, bool> &pr) {
    if (!pr.second) {
        return nullptr;
    }
    return static_cast<BufferSock *>(pr.first.get());
}

ssize_t BufferSendTo::send(int fd, int flags) {
    size_t sent = 0;
    ssize_t n;
    while (!_pkt_list.empty()) {
        auto &front = _pkt_list.front();
        auto &buffer = front.first;
        if (_is_udp) {
            auto ptr = getBufferSockPtr(front);
            n = ::sendto(fd, buffer->data() + _offset, buffer->size() - _offset, flags, ptr ? ptr->sockaddr() : nullptr, ptr ? ptr->socklen() : 0);
        } else {
            n = ::send(fd, buffer->data() + _offset, buffer->size() - _offset, flags);
        }

        if (n >= 0) {
            assert(n);
            _offset += n;
            if (_offset == buffer->size()) {
                if (_cb) {
                    _cb(buffer, true);
                }
                _pkt_list.pop_front();
                _offset = 0;
            }
            sent += n;
            continue;
        }

        //n == -1的情况
        if (get_uv_error(true) == UV_EINTR) {
            //被打断，需要继续发送
            continue;
        }
        //其他原因导致的send返回-1
        break;
    }
    return sent ? sent : -1;
}

//////////////////////////////////////////////////////////////////////////////

BufferList::Ptr BufferList::create(List<std::pair<Buffer::Ptr, bool> > list, SendResult cb, bool is_udp) {
#if defined(_WIN32)
    //wind32目前未做网络发送性能优化
    return std::make_shared<BufferSendTo>(std::move(list), std::move(cb), is_udp);
#elif defined(__linux__) || defined(__linux)
    if (is_udp) {
        //linux后续可以使用sendmmsg优化发送性能
        return std::make_shared<BufferSendTo>(std::move(list), std::move(cb), is_udp);
    }
    return std::make_shared<BufferSendMsg>(std::move(list), std::move(cb));
#else
    if (is_udp) {
        return std::make_shared<BufferSendTo>(std::move(list), std::move(cb), is_udp);
    }
    return std::make_shared<BufferSendMsg>(std::move(list), std::move(cb));
#endif
}

} //toolkit