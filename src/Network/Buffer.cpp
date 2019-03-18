//
// Created by xzl on 2019/3/18.
//

#include <sys/uio.h>
#include "Buffer.h"

namespace toolkit {
///////////////Packet/////////////////////
void Packet::updateStamp(){
    _stamp = (uint32_t)time(NULL);
}
uint32_t Packet::getStamp() const{
    return _stamp;
}

///////////////PacketList/////////////////////
bool PacketList::empty() {
    return _iovec_off == _iovec.size();
}

int PacketList::send(int fd,int flags) {
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

void PacketList::reOffset(int n) {
    _remainSize -= n;
    int offset;
    for(int i = _iovec_off ; i != _iovec.size() ; ++i ){
        auto &ref = _iovec[i];
        offset += ref.iov_len;
        if(offset < n){
            continue;
        }
        ref.iov_len -= (offset - n);
        ref.iov_base = (char *)ref.iov_base + (offset - n);
        _iovec_off = i;
        if(ref.iov_len == 0){
            _iovec_off += 1;
        }
        break;
    }
}
PacketList::PacketList(List<Packet::Ptr> &list) : _iovec(list.size()) {
    _pkt_list.swap(list);
    auto it = _iovec.begin();
    _pkt_list.for_each([&](Packet::Ptr &pkt){
        it->iov_base = pkt->_data->data();
        it->iov_len = pkt->_data->size();
        _remainSize += it->iov_len;
        ++it;
    });
}

uint32_t PacketList::getStamp() {
    return _pkt_list[_iovec_off]->getStamp();
}


}//namespace toolkit