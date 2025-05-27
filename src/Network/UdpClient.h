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

    void onRecvFrom(const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
        if (_on_recvfrom) {
            _on_recvfrom(buf, addr, addr_len);
        }
    }

    void onRecv(const Buffer::Ptr &buf) override {
    }

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

} /* namespace toolkit */
#endif /* NETWORK_UDPCLIENT_H */
