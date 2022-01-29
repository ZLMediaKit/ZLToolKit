/*
 * Copyright (c) 2021 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Server.h"

using namespace std;

namespace toolkit {

Server::Server(EventPoller::Ptr poller) {
    _poller = poller ? std::move(poller) : EventPollerPool::Instance().getPoller();
}

////////////////////////////////////////////////////////////////////////////////////

SessionHelper::SessionHelper(const std::weak_ptr<Server> &server, Session::Ptr session) {
    _server = server;
    _session = std::move(session);
    //记录session至全局的map，方便后面管理
    _session_map = SessionMap::Instance().shared_from_this();
    _identifier = _session->getIdentifier();
    _session_map->add(_identifier, _session);
}

SessionHelper::~SessionHelper() {
    if (!_server.lock()) {
        //务必通知TcpSession已从TcpServer脱离
        _session->onError(SockException(Err_other, "Server shutdown!"));
    }
    //从全局map移除相关记录
    _session_map->del(_identifier);
}

const Session::Ptr &SessionHelper::session() const {
    return _session;
}

////////////////////////////////////////////////////////////////////////////////////

bool SessionMap::add(const string &tag, const Session::Ptr &session) {
    lock_guard<mutex> lck(_mtx_session);
    return _map_session.emplace(tag, session).second;
}

bool SessionMap::del(const string &tag) {
    lock_guard<mutex> lck(_mtx_session);
    return _map_session.erase(tag);
}

Session::Ptr SessionMap::get(const string &tag) {
    lock_guard<mutex> lck(_mtx_session);
    auto it = _map_session.find(tag);
    if (it == _map_session.end()) {
        return nullptr;
    }
    return it->second.lock();
}

void SessionMap::for_each_session(const function<void(const string &id, const Session::Ptr &session)> &cb) {
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

} // namespace toolkit