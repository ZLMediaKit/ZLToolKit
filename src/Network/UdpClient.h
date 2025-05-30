/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef NETWORK_UDPCLIENT_H
#define NETWORK_UDPCLIENT_H

#include <memory>
#include "Socket.h"
#include "Util/SSLBox.h"
#include "Kcp.h"

namespace toolkit {

//Udp客户端，Socket对象默认开始互斥锁
class UdpClient : public SocketHelper {
public:
    using Ptr = std::shared_ptr<UdpClient>;
    using OnRecvFrom = std::function<void(const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len)>;
    using OnErr = std::function<void(const SockException &)>;

    UdpClient(const EventPoller::Ptr &poller = nullptr);
    ~UdpClient() override;

    /**
     * 开始连接udp服务器
     * @param peer_host 服务器ip或域名
     * @param peer_port 服务器端口
     * @param local_port 本地端口
     */
    virtual void startConnect(const std::string &peer_host, uint16_t peer_port, uint16_t local_port = 0);

    /**
     * 主动断开连接
     * @param ex 触发onErr事件时的参数
     */
    void shutdown(const SockException &ex = SockException(Err_shutdown, "self shutdown")) override;

    /**
     * 连接中或已连接返回true，断开连接时返回false
     */
    virtual bool alive() const;

    /**
     * 设置网卡适配器,使用该网卡与服务器通信
     * @param local_ip 本地网卡ip
     */
    virtual void setNetAdapter(const std::string &local_ip);

    /**
     * 唯一标识
     */
    std::string getIdentifier() const override;

    void setOnRecvFrom(OnRecvFrom cb) {
        _on_recvfrom = std::move(cb);
    }

    void setOnError(OnErr cb) {
        _on_err = std::move(cb);
    }

protected:

    virtual void onRecvFrom(const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
        if (_on_recvfrom) {
            _on_recvfrom(buf, addr, addr_len);
        }
    }

    void onRecv(const Buffer::Ptr &buf) override {}

    void onError(const SockException &err) override {
        DebugL;
        if (_on_err) {
            _on_err(err);
        }
    }
 
    /**
     * udp连接成功后每2秒触发一次该事件
     */
    void onManager() override {}

private:
    mutable std::string _id;
    std::string _net_adapter = "::";
    std::shared_ptr<Timer> _timer;
    //对象个数统计
    ObjectStatistic<UdpClient> _statistic;

    OnRecvFrom _on_recvfrom;
    OnErr _on_err;
};

//用于实现KCP客户端的模板对象
template<typename UdpClientType>
class UdpClientWithKcp : public UdpClientType {
public:
    using Ptr = std::shared_ptr<UdpClientWithKcp>;

    template<typename ...ArgsType>
    UdpClientWithKcp(ArgsType &&...args)
        :UdpClientType(std::forward<ArgsType>(args)...) {
        _kcp_box = std::make_shared<KcpTransport>(false);
        _kcp_box->setOnWrite([&](const Buffer::Ptr &buf) { public_send(buf); });
        _kcp_box->setOnRead([&](const Buffer::Ptr &buf) { public_onRecv(buf); });
    }

    ~UdpClientWithKcp() override { }

    void onRecvFrom(const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
        //KCP 暂不支持一个UDP Socket 对多个目标,因此先忽略addr参数
        _kcp_box->input(buf);
    }

    ssize_t send(Buffer::Ptr buf) override {
        return _kcp_box->send(std::move(buf));
    }

    ssize_t sendto(Buffer::Ptr buf, struct sockaddr *addr = nullptr, socklen_t addr_len = 0) override {
        //KCP 暂不支持一个UDP Socket 对多个目标,因此先忽略addr参数
        return _kcp_box->send(std::move(buf));
    }

    inline void public_onRecv(const Buffer::Ptr &buf) {
        //KCP 暂不支持一个UDP Socket 对多个目标,因此固定采用bind的地址参数
        UdpClientType::onRecvFrom(buf, (struct sockaddr*)&_peer_addr, _peer_addr_len);
    }

    inline void public_send(const Buffer::Ptr &buf) {
        UdpClientType::send(buf);
    }

    virtual void startConnect(const std::string &peer_host, uint16_t peer_port, uint16_t local_port = 0) override {
        _peer_addr = SockUtil::make_sockaddr(peer_host.data(), peer_port);
        _peer_addr_len = SockUtil::get_sock_len((const struct sockaddr*)&_peer_addr);
        UdpClientType::startConnect(peer_host, peer_port, local_port);
    }

private:
    struct sockaddr_storage _peer_addr;
    int _peer_addr_len = 0;
    KcpTransport::Ptr _kcp_box;
};

} /* namespace toolkit */
#endif /* NETWORK_UDPCLIENT_H */
