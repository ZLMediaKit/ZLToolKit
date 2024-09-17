/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TCPSERVER_TCPSERVER_H
#define TCPSERVER_TCPSERVER_H

#include <memory>
#include <functional>
#include <unordered_map>
#include "Server.h"
#include "Session.h"
#include "Poller/Timer.h"
#include "Util/util.h"

namespace toolkit {

//TCP服务器，可配置的；配置通过Session::attachServer方法传递给会话对象  [AUTO-TRANSLATED:4e55c332]
//Configurable TCP server; configuration is passed to the session object through the Session::attachServer method
class TcpServer : public Server {
public:
    using Ptr = std::shared_ptr<TcpServer>;

    /**
     * 创建tcp服务器，listen fd的accept事件会加入到所有的poller线程中监听
     * 在调用TcpServer::start函数时，内部会创建多个子TcpServer对象，
     * 这些子TcpServer对象通过Socket对象克隆的方式在多个poller线程中监听同一个listen fd
     * 这样这个TCP服务器将会通过抢占式accept的方式把客户端均匀的分布到不同的poller线程
     * 通过该方式能实现客户端负载均衡以及提高连接接收速度
     * Creates a TCP server, the accept event of the listen fd will be added to all poller threads for listening
     * When calling the TcpServer::start function, multiple child TcpServer objects will be created internally,
     * These child TcpServer objects will be cloned through the Socket object in multiple poller threads to listen to the same listen fd
     * This way, the TCP server will distribute clients evenly across different poller threads through a preemptive accept approach
     * This approach can achieve client load balancing and improve connection acceptance speed
     
     * [AUTO-TRANSLATED:761a6b1e]
     */
    explicit TcpServer(const EventPoller::Ptr &poller = nullptr);
    ~TcpServer() override;

    /**
    * @brief 开始tcp server
    * @param port 本机端口，0则随机
    * @param host 监听网卡ip
    * @param backlog tcp listen backlog
     * @brief Starts the TCP server
     * @param port Local port, 0 for random
     * @param host Listening network card IP
     * @param backlog TCP listen backlog
     
     * [AUTO-TRANSLATED:9bab69b6]
    */
    template <typename SessionType>
    void start(uint16_t port, const std::string &host = "::", uint32_t backlog = 1024, const std::function<void(std::shared_ptr<SessionType> &)> &cb = nullptr) {
        static std::string cls_name = toolkit::demangle(typeid(SessionType).name());
        // Session创建器，通过它创建不同类型的服务器  [AUTO-TRANSLATED:f5585e1e]
        //Session creator, creates different types of servers through it
        _session_alloc = [cb](const TcpServer::Ptr &server, const Socket::Ptr &sock) {
            auto session = std::shared_ptr<SessionType>(new SessionType(sock), [](SessionType *ptr) {
                TraceP(static_cast<Session *>(ptr)) << "~" << cls_name;
                delete ptr;
            });
            if (cb) {
                cb(session);
            }
            TraceP(static_cast<Session *>(session.get())) << cls_name;
            session->setOnCreateSocket(server->_on_create_socket);
            return std::make_shared<SessionHelper>(server, std::move(session), cls_name);
        };
        start_l(port, host, backlog);
    }

    /**
     * @brief 获取服务器监听端口号, 服务器可以选择监听随机端口
     * @brief Gets the server listening port number, the server can choose to listen on a random port
     
     * [AUTO-TRANSLATED:125ff8d8]
     */
    uint16_t getPort();

    /**
     * @brief 自定义socket构建行为
     * @brief Custom socket construction behavior
     
     * [AUTO-TRANSLATED:4cf98e86]
     */
    void setOnCreateSocket(Socket::onCreateSocket cb);

    /**
     * 根据socket对象创建Session对象
     * 需要确保在socket归属poller线程执行本函数
     * Creates a Session object based on the socket object
     * Ensures that this function is executed in the poller thread that owns the socket
     
     * [AUTO-TRANSLATED:1d52d9ee]
     */
    Session::Ptr createSession(const Socket::Ptr &socket);

protected:
    virtual void cloneFrom(const TcpServer &that);
    virtual TcpServer::Ptr onCreatServer(const EventPoller::Ptr &poller);

    virtual Session::Ptr onAcceptConnection(const Socket::Ptr &sock);
    virtual Socket::Ptr onBeforeAcceptConnection(const EventPoller::Ptr &poller);

private:
    void onManagerSession();
    Socket::Ptr createSocket(const EventPoller::Ptr &poller);
    void start_l(uint16_t port, const std::string &host, uint32_t backlog);
    Ptr getServer(const EventPoller *) const;
    void setupEvent();

private:
    bool _multi_poller;
    bool _is_on_manager = false;
    bool _main_server = true;
    std::weak_ptr<TcpServer> _parent;
    Socket::Ptr _socket;
    std::shared_ptr<Timer> _timer;
    Socket::onCreateSocket _on_create_socket;
    std::unordered_map<SessionHelper *, SessionHelper::Ptr> _session_map;
    std::function<SessionHelper::Ptr(const TcpServer::Ptr &server, const Socket::Ptr &)> _session_alloc;
    std::unordered_map<const EventPoller *, Ptr> _cloned_server;
    //对象个数统计  [AUTO-TRANSLATED:3b43e8c2]
    //Object count statistics
    ObjectStatistic<TcpServer> _statistic;
};

} /* namespace toolkit */
#endif /* TCPSERVER_TCPSERVER_H */
