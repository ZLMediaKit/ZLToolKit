/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TCPSERVER_TCPSERVER_H
#define TCPSERVER_TCPSERVER_H

#include <mutex>
#include <memory>
#include <exception>
#include <functional>
#include <unordered_map>
#include "TcpSession.h"
#include "Util/mini.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/uv_errno.h"
#include "Poller/Timer.h"
#include "Thread/semaphore.h"
using namespace std;

namespace toolkit {

//全局的TcpSession记录对象，方便后面管理
//线程安全的
class SessionMap : public std::enable_shared_from_this<SessionMap> {
public:
    friend class TcpSessionHelper;
    typedef std::shared_ptr<SessionMap> Ptr;
    //单例
    static SessionMap &Instance();
    ~SessionMap(){};

    //获取Session
    TcpSession::Ptr get(const string &tag){
        lock_guard<mutex> lck(_mtx_session);
        auto it = _map_session.find(tag);
        if(it == _map_session.end()){
            return nullptr;
        }
        return it->second.lock();
    }
    void for_each_session(const function<void(const string &id,const TcpSession::Ptr &session)> &cb){
        lock_guard<mutex> lck(_mtx_session);
        for(auto it = _map_session.begin() ; it != _map_session.end() ; ){
            auto session = it->second.lock();
            if(!session){
                it = _map_session.erase(it);
                continue;
            }
            cb(it->first,session);
            ++it;
        }
    }
private:
    SessionMap(){};
    //添加Session
    bool add(const string &tag,const TcpSession::Ptr &session){
        //InfoL ;
        lock_guard<mutex> lck(_mtx_session);
        return _map_session.emplace(tag,session).second;
    }
    //移除Session
    bool del(const string &tag){
        //InfoL ;
        lock_guard<mutex> lck(_mtx_session);
        return _map_session.erase(tag);
    }
private:
    unordered_map<string, weak_ptr<TcpSession> > _map_session;
    mutex _mtx_session;
};

class TcpServer;
class TcpSessionHelper {
public:
    typedef std::shared_ptr<TcpSessionHelper> Ptr;

    TcpSessionHelper(const std::weak_ptr<TcpServer> &server,TcpSession::Ptr &&session){
        _server = server;
        _session = std::move(session);
        //记录session至全局的map，方便后面管理
        _session_map = SessionMap::Instance().shared_from_this();
        _identifier = _session->getIdentifier();
        _session_map->add(_identifier,_session);
    }
    ~TcpSessionHelper(){
        if(!_server.lock()){
            //务必通知TcpSession已从TcpServer脱离
            _session->onError(SockException(Err_other,"Tcp server shutdown!"));
        }
        //从全局map移除相关记录
        _session_map->del(_identifier);
    }

    const TcpSession::Ptr &session() const{
        return _session;
    }
private:
    std::weak_ptr<TcpServer> _server;
    TcpSession::Ptr _session;
    SessionMap::Ptr _session_map;
    string _identifier;
};


//TCP服务器，可配置的；配置通过TcpSession::attachServer方法传递给会话对象
//该对象是非线程安全的，务必在主线程中操作
class TcpServer : public mINI , public std::enable_shared_from_this<TcpServer>{
public:
    typedef std::shared_ptr<TcpServer> Ptr;

    /**
     * 创建tcp服务器，listen fd的accept事件会加入到所有的poller线程中监听
     * 在调用TcpServer::start函数时，内部会创建多个子TcpServer对象，
     * 这些子TcpServer对象通过Socket对象克隆的方式在多个poller线程中监听同一个listen fd
     * 这样这个TCP服务器将会通过抢占式accept的方式把客户端均匀的分布到不同的poller线程
     * 通过该方式能实现客户端负载均衡以及提高连接接收速度
     */
    TcpServer(const EventPoller::Ptr &poller = nullptr) {
        _poller = poller;
        if(!_poller){
            _poller =  EventPollerPool::Instance().getPoller();
        }
        _socket = std::make_shared<Socket>(_poller);
        _socket->setOnAccept(bind(&TcpServer::onAcceptConnection_l, this, placeholders::_1));
        _socket->setOnBeforeAccept(bind(&TcpServer::onBeforeAcceptConnection_l, this,std::placeholders::_1));
    }

    virtual ~TcpServer() {
        _timer.reset();
        //先关闭socket监听，防止收到新的连接
        _socket.reset();

        if(!_cloned) {
            TraceL << "start clean tcp server...";
        }
        _sessionMap.clear();
        _clonedServer.clear();
        if(!_cloned){
            TraceL << "clean tcp server completed!";
        }
    }

    //开始监听服务器
    template <typename SessionType>
    void start(uint16_t port, const std::string& host = "0.0.0.0", uint32_t backlog = 1024) {
        start_l<SessionType>(port,host,backlog);
        EventPollerPool::Instance().for_each([&](const TaskExecutor::Ptr &executor){
            EventPoller::Ptr poller = dynamic_pointer_cast<EventPoller>(executor);
            if(poller == _poller || !poller){
                return;
            }
            auto &serverRef = _clonedServer[poller.get()];
            if(!serverRef){
                serverRef = onCreatServer(poller);
            }
            if(serverRef){
                serverRef->cloneFrom(*this);
            }
        });
    }

    /**
     * 获取服务器监听端口号，服务器可以选择监听随机端口
     */
    uint16_t getPort(){
        if(!_socket){
            return 0;
        }
        return _socket->get_local_port();
    }

    /**
     * 服务器器模型socket是线程安全的，所以为了提高性能，一般关闭互斥锁
     * @param flag 是否使能socket互斥锁，默认关闭
     */
    void enableSocketMutex(bool flag){
        _enableSocketMutex = flag;
    }
protected:
    virtual TcpServer::Ptr onCreatServer(const EventPoller::Ptr &poller){
        return std::make_shared<TcpServer>(poller);
    }

    virtual Socket::Ptr onBeforeAcceptConnection(const EventPoller::Ptr &poller){
        return std::make_shared<Socket>(poller,_enableSocketMutex);
    }

    virtual void cloneFrom(const TcpServer &that){
        if(!that._socket){
            throw std::invalid_argument("TcpServer::cloneFrom other with null socket!");
        }
        _sessionMaker = that._sessionMaker;
        _socket->cloneFromListenSocket(*(that._socket));
        _timer = std::make_shared<Timer>(2, [this]()->bool {
            this->onManagerSession();
            return true;
        },_poller);
        this->mINI::operator=(that);
        _cloned = true;
        _enableSocketMutex = that._enableSocketMutex;
    }

    // 接收到客户端连接请求
    virtual void onAcceptConnection(const Socket::Ptr & sock) {
        weak_ptr<TcpServer> weakSelf = shared_from_this();
        //创建一个TcpSession;这里实现创建不同的服务会话实例
        auto sessionHelper = _sessionMaker(weakSelf,sock);
        auto &session = sessionHelper->session();
        //把本服务器的配置传递给TcpSession
        session->attachServer(*this);

        //_sessionMap::emplace肯定能成功
        auto success = _sessionMap.emplace(sessionHelper.get(), sessionHelper).second;
        assert(success == true);

        weak_ptr<TcpSession> weakSession(session);
        //会话接收数据事件
        sock->setOnRead([weakSession](const Buffer::Ptr &buf, struct sockaddr *, int ){
            //获取会话强应用
            auto strongSession=weakSession.lock();
            if(!strongSession) {
                //会话对象已释放
                return;
            }
            strongSession->onRecv(buf);
        });

        TcpSessionHelper *ptr = sessionHelper.get();
        //会话接收到错误事件
        sock->setOnErr([weakSelf,weakSession,ptr](const SockException &err){
            //在本函数作用域结束时移除会话对象
            //目的是确保移除会话前执行其onError函数
            //同时避免其onError函数抛异常时没有移除会话对象
            onceToken token(nullptr, [ptr, &weakSelf]() {
                //移除掉会话
                auto strongSelf = weakSelf.lock();
                if (!strongSelf) {
                    return;
                }
                //在TcpServer对应线程中移除map相关记录
                strongSelf->_poller->async([weakSelf, ptr]() {
                    auto strongSelf = weakSelf.lock();
                    if (!strongSelf) {
                        return;
                    }
                    strongSelf->_sessionMap.erase(ptr);
                });
            });

            //获取会话强应用
            auto strongSession = weakSession.lock();
            if(strongSession) {
                //触发onError事件回调
                strongSession->onError(err);
            }
        });
    }

private:
    Socket::Ptr onBeforeAcceptConnection_l(const EventPoller::Ptr &poller){
        return onBeforeAcceptConnection(poller);
    }
    // 接收到客户端连接请求
    void onAcceptConnection_l(const Socket::Ptr & sock) {
        onAcceptConnection(sock);
    }


    template <typename SessionType>
    void start_l(uint16_t port, const std::string& host = "0.0.0.0", uint32_t backlog = 1024) {
        //TcpSession创建器，通过它创建不同类型的服务器
        _sessionMaker = [](const weak_ptr<TcpServer> &server,const Socket::Ptr &sock){
            return std::make_shared<TcpSessionHelper>(server,std::make_shared<SessionType>(sock));
        };

        if (!_socket->listen(port, host.c_str(), backlog)) {
            //创建tcp监听失败，可能是由于端口占用或权限问题
            string err = (StrPrinter << "listen on " << host << ":" << port << " failed:" << get_uv_errmsg(true));
            throw std::runtime_error(err);
        }

        //新建一个定时器定时管理这些tcp会话
        _timer = std::make_shared<Timer>(2, [this]()->bool {
            this->onManagerSession();
            return true;
        },_poller);
        InfoL << "TCP Server listening on " << host << ":" << port;
    }

    //定时管理Session
    void onManagerSession() {
        for (auto &pr : _sessionMap) {
            weak_ptr<TcpSession> weakSession = pr.second->session();
            pr.second->session()->async([weakSession]() {
                auto strongSession=weakSession.lock();
                if(!strongSession) {
                    return;
                }
                strongSession->onManager();
            }, false);
        }
    }
private:
    EventPoller::Ptr _poller;
    Socket::Ptr _socket;
    std::shared_ptr<Timer> _timer;
    unordered_map<TcpSessionHelper *, TcpSessionHelper::Ptr > _sessionMap;
    function<TcpSessionHelper::Ptr(const weak_ptr<TcpServer> &server,const Socket::Ptr &)> _sessionMaker;
    unordered_map<EventPoller *,Ptr> _clonedServer;
    bool _cloned = false;
    bool _enableSocketMutex = false;
};

} /* namespace toolkit */
#endif /* TCPSERVER_TCPSERVER_H */
