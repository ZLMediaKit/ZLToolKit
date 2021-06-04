/*
 * Copyright (c) 2021 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLTOOLKIT_SERVER_H
#define ZLTOOLKIT_SERVER_H

#include <unordered_map>

#include "Network/Session.h"
#include "Util/mini.h"

namespace toolkit {

// 全局的 Session 记录对象, 方便后面管理
// 线程安全的
class SessionMap : public std::enable_shared_from_this<SessionMap> {
public:
    friend class SessionHelper;
    typedef std::shared_ptr<SessionMap> Ptr;

    //单例
    static SessionMap &Instance();
    ~SessionMap() {};

    //获取Session
    Session::Ptr get(const string &tag) {
        lock_guard<mutex> lck(_mtx_session);
        auto it = _map_session.find(tag);
        if (it == _map_session.end()) {
            return nullptr;
        }
        return it->second.lock();
    }

    void for_each_session(const function<void(const string &id, const Session::Ptr &session)> &cb) {
        lock_guard<mutex> lck(_mtx_session);
        for (auto it = _map_session.begin(); it != _map_session.end();) {
            auto session = it->second.lock();
            if (!session) {
                it = _map_session.erase(it);
                continue;
            }
            cb(it->first, session);
            ++it;
        }
    }

private:
    SessionMap() {};

    //添加Session
    bool add(const string &tag, const Session::Ptr &session) {
        lock_guard<mutex> lck(_mtx_session);
        return _map_session.emplace(tag, session).second;
    }

    //移除Session
    bool del(const string &tag) {
        lock_guard<mutex> lck(_mtx_session);
        return _map_session.erase(tag);
    }

private:
    mutex _mtx_session;
    unordered_map<string, weak_ptr<Session> > _map_session;
};

class Server;
class SessionHelper {
public:
    typedef std::shared_ptr<SessionHelper> Ptr;

    SessionHelper(const std::weak_ptr<Server> &server, Session::Ptr session) {
        _server = server;
        _session = std::move(session);
        //记录session至全局的map，方便后面管理
        _session_map = SessionMap::Instance().shared_from_this();
        _identifier = _session->getIdentifier();
        _session_map->add(_identifier, _session);
    }

    ~SessionHelper(){
        if (!_server.lock()) {
            //务必通知TcpSession已从TcpServer脱离
            _session->onError(SockException(Err_other, "Server shutdown!"));
        }
        //从全局map移除相关记录
        _session_map->del(_identifier);
    }

    const Session::Ptr &session() const { return _session; }

private:
    string _identifier;
    Session::Ptr _session;
    SessionMap::Ptr _session_map;
    std::weak_ptr<Server> _server;
};


//
// server 基类, 暂时仅用于剥离 SessionHelper 对 TcpServer 的依赖
// 后续将 TCP 与 UDP 服务通用部分加到这里.
//
class Server : public std::enable_shared_from_this<Server>, public mINI {
public:
    typedef std::shared_ptr<Server> Ptr;

    explicit Server(EventPoller::Ptr poller = nullptr);
    virtual ~Server();

protected:
    EventPoller::Ptr _poller;
};

} // namespace toolkit

#endif // ZLTOOLKIT_SERVER_H
