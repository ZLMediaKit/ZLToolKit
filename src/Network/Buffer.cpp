//
// Created by xzl on 2019/3/18.
//

#include <sys/uio.h>
#include "Buffer.h"

namespace toolkit {
///////////////BufferList/////////////////////
bool BufferList::empty() {
    return _iovec_off == _iovec.size();
}

int BufferList::send(int fd,int flags) {
    int n;
    do {
        struct msghdr msg;
        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        msg.msg_iov = &(_iovec[_iovec_off]);
        msg.msg_iovlen = _iovec.size() - _iovec_off;
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        msg.msg_flags = flags;
        n = sendmsg(fd,&msg,flags);
    } while (-1 == n && UV_EINTR == get_uv_error(true));

    if(n >= _remainSize){
        //全部写完了
        _iovec_off = _iovec.size();
        return n;
    }

    if(n > 0){
        //部分发送成功
        reOffset(n);
        return n;
    }

    //一个字节都未发送
    return n;
}

void BufferList::reOffset(int n) {
    _remainSize -= n;
    int offset = 0;
    int last_off = _iovec_off;
    for(int i = _iovec_off ; i != _iovec.size() ; ++i ){
        auto &ref = _iovec[i];
        offset += ref.iov_len;
        if(offset < n){
            continue;
        }
        int remain = offset - n;
        ref.iov_base = (char *)ref.iov_base + ref.iov_len - remain;
        ref.iov_len = remain;
        _iovec_off = i;
        if(remain == 0){
            _iovec_off += 1;
        }
        break;
    }

    //删除已经发送的数据，节省内存
    for (int i = last_off ; i < _iovec_off ; ++i){
        _pkt_list.pop_front();
    }
}
BufferList::BufferList(List<Buffer::Ptr> &list) : _iovec(list.size()) {
    _pkt_list.swap(list);
    auto it = _iovec.begin();
    _pkt_list.for_each([&](Buffer::Ptr &pkt){
        it->iov_base = pkt->data();
        it->iov_len = pkt->size();
        _remainSize += it->iov_len;
        ++it;
    });
}

}//namespace toolkit