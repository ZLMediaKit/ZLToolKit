/*
 * Copyright (c) 2021 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TOOLKIT_NETWORK_UDPSERVER_H
#define TOOLKIT_NETWORK_UDPSERVER_H

#if __cplusplus >= 201703L
#include <array>
#include <string_view>
#endif
#include "Server.h"
#include "Session.h"

namespace toolkit {

class UdpServer : public Server {
public:
#if __cplusplus >= 201703L
    class PeerIdType : public std::array<char, 18> {
#else
    class PeerIdType : public std::string {
#endif
    public:
#if __cplusplus < 201703L
        PeerIdType() {
            resize(18);
        }
#endif
        bool operator==(const PeerIdType &that) const {
            return as<uint64_t>(0) == that.as<uint64_t>(0) &&
                   as<uint64_t>(8) == that.as<uint64_t>(8) &&
                   as<uint16_t>(16) == that.as<uint16_t>(16);
        }

    private:
        template <class T>
        const T& as(size_t offset) const {
            return *(reinterpret_cast<const T *>(data() + offset));
        }
    };

    using Ptr = std::shared_ptr<UdpServer>;
    using onCreateSocket = std::function<Socket::Ptr(const EventPoller::Ptr &, const Buffer::Ptr &, struct sockaddr *, int)>;

    explicit UdpServer(const EventPoller::Ptr &poller = nullptr);
    ~UdpServer() override;

    /**
     * @brief 开始监听服务器
     * @brief Start listening to the server
     
     * [AUTO-TRANSLATED:342e9d0e]
     */
    template<typename SessionType>
    void start(uint16_t port, const std::string &host = "::", const std::function<void(std::shared_ptr<SessionType> &)> &cb = nullptr) {
        static std::string cls_name = toolkit::demangle(typeid(SessionType).name());
        // Session 创建器, 通过它创建不同类型的服务器  [AUTO-TRANSLATED:a419bcd3]
        //Session creator, creates different types of servers through it
        _session_alloc = [cb](const UdpServer::Ptr &server, const Socket::Ptr &sock) {
            auto session = std::shared_ptr<SessionType>(new SessionType(sock), [](SessionType * ptr) {
                TraceP(static_cast<Session *>(ptr)) << "~" << cls_name;
                delete ptr;
            });
            if (cb) {
                cb(session);
            }
            TraceP(static_cast<Session *>(session.get())) << cls_name;
            auto sock_creator = server->_on_create_socket;
            session->setOnCreateSocket([sock_creator](const EventPoller::Ptr &poller) {
                return sock_creator(poller, nullptr, nullptr, 0);
            });
            return std::make_shared<SessionHelper>(server, std::move(session), cls_name);
        };
        start_l(port, host);
    }

    /**
     * @brief 获取服务器监听端口号, 服务器可以选择监听随机端口
     * @brief Get the server listening port number, the server can choose to listen to a random port
     
     * [AUTO-TRANSLATED:125ff8d8]
     */
    uint16_t getPort();

    /**
     * @brief 自定义socket构建行为
     * @brief Custom socket construction behavior
     
     * [AUTO-TRANSLATED:4cf98e86]
     */
    void setOnCreateSocket(onCreateSocket cb);

protected:
    virtual Ptr onCreatServer(const EventPoller::Ptr &poller);
    virtual void cloneFrom(const UdpServer &that);

private:
    struct PeerIdHash {
#if __cplusplus >= 201703L
        size_t operator()(const PeerIdType &v) const noexcept { return std::hash<std::string_view> {}(std::string_view(v.data(), v.size())); }
#else
        size_t operator()(const PeerIdType &v) const noexcept { return std::hash<std::string> {}(v); }
#endif
    };
    using SessionMapType = std::unordered_map<PeerIdType, SessionHelper::Ptr, PeerIdHash>;

    /**
     * @brief 开始udp server
     * @param port 本机端口，0则随机
     * @param host 监听网卡ip
     * @brief Start UDP server
     * @param port Local port, 0 for random
     * @param host Listening network card IP
     
     * [AUTO-TRANSLATED:1c46778d]
     */
    void start_l(uint16_t port, const std::string &host = "::");

    /**
     * @brief 定时管理 Session, UDP 会话需要根据需要处理超时
     * @brief Periodically manage Session, UDP sessions need to handle timeouts as needed
     
     * [AUTO-TRANSLATED:86ff2f9c]
     */
    void onManagerSession();

    void onRead(Buffer::Ptr &buf, struct sockaddr *addr, int addr_len);

    /**
     * @brief 接收到数据,可能来自server fd，也可能来自peer fd
     * @param is_server_fd 时候为server fd
     * @param id 客户端id
     * @param buf 数据
     * @param addr 客户端地址
     * @param addr_len 客户端地址长度
     * @brief Receive data, may come from server fd or peer fd
     * @param is_server_fd Whether it is a server fd
     * @param id Client ID
     * @param buf Data
     * @param addr Client address
     * @param addr_len Client address length
     
     * [AUTO-TRANSLATED:1c02c9de]
     */
    void onRead_l(bool is_server_fd, const PeerIdType &id, Buffer::Ptr &buf, struct sockaddr *addr, int addr_len);

    /**
     * @brief 根据对端信息获取或创建一个会话
     * @brief Get or create a session based on peer information
     
     * [AUTO-TRANSLATED:c7e1f0c3]
     */
    SessionHelper::Ptr getOrCreateSession(const PeerIdType &id, Buffer::Ptr &buf, struct sockaddr *addr, int addr_len, bool &is_new);

    /**
     * @brief 创建一个会话, 同时进行必要的设置
     * @brief Create a session and perform necessary settings
     
     * [AUTO-TRANSLATED:355c4256]
     */
    SessionHelper::Ptr createSession(const PeerIdType &id, Buffer::Ptr &buf, struct sockaddr *addr, int addr_len);

    /**
     * @brief 创建socket
     * @brief Create a socket
     
     * [AUTO-TRANSLATED:c9aacad4]
     */
    Socket::Ptr createSocket(const EventPoller::Ptr &poller, const Buffer::Ptr &buf = nullptr, struct sockaddr *addr = nullptr, int addr_len = 0);

    void setupEvent();

private:
    bool _cloned = false;
    bool _multi_poller;
    Socket::Ptr _socket;
    std::shared_ptr<Timer> _timer;
    onCreateSocket _on_create_socket;
    //cloned server共享主server的session map，防止数据在不同server间漂移  [AUTO-TRANSLATED:9a149e52]
    //Cloned server shares the session map with the main server, preventing data drift between different servers
    std::shared_ptr<std::recursive_mutex> _session_mutex;
    std::shared_ptr<SessionMapType> _session_map;
    //主server持有cloned server的引用  [AUTO-TRANSLATED:04a6403a]
    //Main server holds a reference to the cloned server
    std::unordered_map<EventPoller *, Ptr> _cloned_server;
    std::function<SessionHelper::Ptr(const UdpServer::Ptr &, const Socket::Ptr &)> _session_alloc;
    // 对象个数统计  [AUTO-TRANSLATED:f4a012d0]
    //Object count statistics
    ObjectStatistic<UdpServer> _statistic;
};

} // namespace toolkit

#endif // TOOLKIT_NETWORK_UDPSERVER_H
