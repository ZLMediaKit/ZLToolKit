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

#include <string>
#include "TcpSession.h"


namespace ZL {
namespace Network {

TcpSession::TcpSession( const std::shared_ptr<ThreadPool> &th,
                        const Socket::Ptr &sock) : _th(th), _sock(sock),SocketWriter(sock) {
    _localIp = _sock->get_local_ip();
    _peerIp = _sock->get_peer_ip();
    _localPort = _sock->get_local_port();
    _peerPort = _sock->get_peer_port();
}

TcpSession::~TcpSession() {
}

string TcpSession::getIdentifier() const{
    return to_string(reinterpret_cast<uint64_t>(this));
}

void TcpSession::safeShutdown(){
    std::weak_ptr<TcpSession> weakSelf = shared_from_this();
    async_first([weakSelf](){
        auto strongSelf = weakSelf.lock();
        if(strongSelf){
            strongSelf->shutdown();
        }
    });
}

const string& TcpSession::getLocalIp() const {
    return _localIp;
}

uint16_t TcpSession::getLocalPort() const {
    return _localPort;
}

const string& TcpSession::getPeerIp() const {
    return _peerIp;
}

uint16_t TcpSession::getPeerPort() const {
    return _peerPort;
}

BufferRaw::Ptr TcpSession::obtainBuffer(){
    return _sock->obtainBuffer();
}
void TcpSession::shutdown() {
    _sock->emitErr(SockException(Err_other, "self shutdown"));
}

int TcpSession::send(const string &buf) {
    return _sock->send(buf,_flags);
}

int TcpSession::send(string &&buf) {
    return _sock->send(std::move(buf),_flags);
}

int TcpSession::send(const char *buf, int size) {
    return _sock->send(buf,size,_flags);
}

int TcpSession::send(const Buffer::Ptr &buf) {
    return _sock->send(buf, _flags);
}


} /* namespace Session */
} /* namespace ZL */

