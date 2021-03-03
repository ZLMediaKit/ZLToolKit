/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xia-chu/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef NETWORK_TCPCLIENT_H
#define NETWORK_TCPCLIENT_H

#include <mutex>
#include <memory>
#include <functional>
#include "Socket.h"
#include "Util/TimeTicker.h"
#include "Util/SSLBox.h"
using namespace std;

namespace toolkit {

//Tcp客户端，Socket对象默认开始互斥锁
class TcpClient : public std::enable_shared_from_this<TcpClient>, public SocketHelper {
public:
    typedef std::shared_ptr<TcpClient> Ptr;
    TcpClient(const EventPoller::Ptr &poller = nullptr);
    ~TcpClient() override;

    /**
     * 开始连接tcp服务器
     * @param url 服务器ip或域名
     * @param port 服务器端口
     * @param timeout_sec 超时时间,单位秒
     */
    virtual void startConnect(const string &url, uint16_t port, float timeout_sec = 5);

    /**
     * 主动断开连接
     * @param ex 触发onErr事件时的参数
     */
    void shutdown(const SockException &ex = SockException(Err_shutdown, "self shutdown")) override;

    /**
     * 判断是否与服务器连接中
     */
    virtual bool alive();

    /**
     * 设置网卡适配器,使用该网卡与服务器通信
     * @param local_ip 本地网卡ip
     */
    virtual void setNetAdapter(const string &local_ip);

protected:
    /**
     * 连接服务器结果回调
     * @param ex 成功与否
     */
    virtual void onConnect(const SockException &ex) {}

    /**
     * 收到数据回调
     * @param buf 接收到的数据(该buffer会重复使用)
     */
    virtual void onRecv(const Buffer::Ptr &buf) {}

    /**
     * 数据全部发送完毕后回调
     */
    virtual void onFlush() {}

    /**
     * 被动断开连接回调
     * @param ex 断开原因
     */
    virtual void onErr(const SockException &ex) {}

    /**
     * tcp连接成功后每2秒触发一次该事件
     */
    virtual void onManager() {}

private:
    void onSockConnect(const SockException &ex);

private:
    string _net_adapter = "0.0.0.0";
    std::shared_ptr<Timer> _timer;
    //对象个数统计
    ObjectStatistic<TcpClient> _statistic;
};

//用于实现TLS客户端的模板对象
template<typename TcpClientType>
class TcpClientWithSSL: public TcpClientType {
public:
    typedef std::shared_ptr<TcpClientWithSSL> Ptr;

    template<typename ...ArgsType>
    TcpClientWithSSL(ArgsType &&...args):TcpClientType(std::forward<ArgsType>(args)...) {}

    ~TcpClientWithSSL() override{
        if (_ssl_box) {
            _ssl_box->flush();
        }
    }

    void onRecv(const Buffer::Ptr &buf) override {
        if (_ssl_box) {
            _ssl_box->onRecv(buf);
        } else {
            TcpClientType::onRecv(buf);
        }
    }

    ssize_t send(Buffer::Ptr buf) override {
        if (_ssl_box) {
            auto size = buf->size();
            _ssl_box->onSend(buf);
            return size;
        }
        return TcpClientType::send(std::move(buf));
    }

    //添加public_onRecv和public_send函数是解决较低版本gcc一个lambad中不能访问protected或private方法的bug
    inline void public_onRecv(const Buffer::Ptr &buf) {
        TcpClientType::onRecv(buf);
    }

    inline void public_send(const Buffer::Ptr &buf) {
        TcpClientType::send(std::move(const_cast<Buffer::Ptr &>(buf)));
    }

    void startConnect(const string &url, uint16_t port, float timeout_sec = 5) override {
        _host = url;
        TcpClientType::startConnect(url, port, timeout_sec);
    }

protected:
    void onConnect(const SockException &ex) override {
        if (!ex) {
            _ssl_box = std::make_shared<SSL_Box>(false);
            _ssl_box->setOnDecData([this](const Buffer::Ptr &buf) {
                public_onRecv(buf);
            });
            _ssl_box->setOnEncData([this](const Buffer::Ptr &buf) {
                public_send(buf);
            });

            if (!isIP(_host.data())) {
                //设置ssl域名
                _ssl_box->setHost(_host.data());
            }
        }
        TcpClientType::onConnect(ex);
    }

private:
    string _host;
    std::shared_ptr<SSL_Box> _ssl_box;
};

} /* namespace toolkit */
#endif /* NETWORK_TCPCLIENT_H */
