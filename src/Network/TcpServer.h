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
#include "Thread/WorkThreadPool.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Poller;
using namespace ZL::Thread;

namespace ZL {
namespace Network {

//全局的TcpSession记录对象，方便后面管理
//线程安全的
class SessionMap {
public:
    friend class TcpServer;
    //单例
    static SessionMap &Instance(){
        static SessionMap instance;
        return instance;
    }
    //获取Session
    TcpSession::Ptr get(const string &tag){
        lock_guard<mutex> lck(_mtx_session);
        auto it = _map_session.find(tag);
        if(it == _map_session.end()){
            return nullptr;
        }
        return it->second.lock();
    }
private:
    SessionMap(){};
    ~SessionMap(){};
    //添加Session
    bool add(const string &tag,const TcpSession::Ptr &session){
        //InfoL ;
        lock_guard<mutex> lck(_mtx_session);
        return _map_session.emplace(tag,session).second;
    }
    //移除Session
    bool remove(const string &tag){
        //InfoL ;
        lock_guard<mutex> lck(_mtx_session);
        return _map_session.erase(tag);
    }
private:
    unordered_map<string, weak_ptr<TcpSession> > _map_session;
    mutex _mtx_session;
};


//TCP服务器，可配置的；配置通过TcpSession::attachServer方法传递给会话对象
//该对象是非线程安全的，务必在主线程中操作
class TcpServer : public mINI {
public:
	typedef std::shared_ptr<TcpServer> Ptr;
    typedef unordered_map<string, TcpSession::Ptr > SessionMapType;

    TcpServer() {
		_socket.reset(new Socket());
		_sessionMap.reset(new SessionMapType);
        _socket->setOnAccept( bind(&TcpServer::onAcceptConnection, this, placeholders::_1));
    }

	~TcpServer() {
		TraceL << "start clean tcp server...";
		_timer.reset();
        //先关闭socket监听，防止收到新的连接
		_socket.reset();

        //务必通知TcpSession已从TcpServer脱离
		for (auto &pr : *_sessionMap) {
            //从全局的map中移除记录
            SessionMap::Instance().remove(pr.first);

            auto session = pr.second;
            session->async_first([session]() {
                //遍历触发onError事件，确保脱离TcpServer时都能知晓
				session->onError(SockException(Err_other,"Tcp server shutdown!"));
			});
		}
		TraceL << "clean tcp server completed!";
	}

    //开始监听服务器
    template <typename SessionType>
	void start(uint16_t port, const std::string& host = "0.0.0.0", uint32_t backlog = 1024) {
        //TcpSession创建器，通过它创建不同类型的服务器
        _sessionMaker = [](const std::shared_ptr<ThreadPool> &th, const Socket::Ptr &sock){
            return std::make_shared<SessionType>(th,sock);
        };

        if (!_socket->listen(port, host.c_str(), backlog)) {
            //创建tcp监听失败，可能是由于端口占用或权限问题
			string err = (StrPrinter << "listen on " << host << ":" << port << " failed:" << get_uv_errmsg(true));
			throw std::runtime_error(err);
		}

        //新建一个定时器定时管理这些tcp会话
		_timer.reset(new Timer(2, [this]()->bool {
			this->onManagerSession();
			return true;
		}));
		InfoL << "TCP Server listening on " << host << ":" << port;
	}
	 uint16_t getPort(){
		 if(!_socket){
			 return 0;
		 }
		 return _socket->get_local_port();
	 }
private:
    // 接收到客户端连接请求
    void onAcceptConnection(const Socket::Ptr & sock) {
        //创建一个TcpSession;这里实现创建不同的服务会话实例
		auto session = _sessionMaker(WorkThreadPool::Instance().getWorkThread(), sock);
        //把本服务器的配置传递给TcpSession
        session->attachServer(*this);

        //TcpSession的唯一识别符，可以是guid之类的
        auto sessionId = session->getIdentifier();
        //记录该TcpSession
        if(!SessionMap::Instance().add(sessionId,session)){
            //有同名session，说明getIdentifier生成的标识符有问题
            WarnL << "SessionMap::add failed:" << sessionId;
            return;
        }
        //SessionMap中没有相关记录，那么_sessionMap更不可能有相关记录了；
        //所以_sessionMap::emplace肯定能成功
        auto success = _sessionMap->emplace(sessionId, session).second;
        assert(success == true);

        weak_ptr<TcpSession> weakSession(session);
		//会话接收数据事件
		sock->setOnRead([weakSession](const Buffer::Ptr &buf, struct sockaddr *addr){
			//获取会话强应用
			auto strongSession=weakSession.lock();
			if(!strongSession) {
				//会话对象已释放
				return;
			}
			//在会话线程中执行onRecv操作
			strongSession->async([weakSession,buf]() {
				auto strongSession=weakSession.lock();
				if(!strongSession) {
					return;
				}
				strongSession->onRecv(buf);
			});
		});

        auto sessionMapTmp = _sessionMap;
        //会话接收到错误事件
		sock->setOnErr([weakSession,sessionId,sessionMapTmp](const SockException &err){
			//获取会话强应用
			auto strongSession=weakSession.lock();
			//移除掉会话
			sessionMapTmp->erase(sessionId);
            SessionMap::Instance().remove(sessionId);
            if(!strongSession) {
				//会话对象已释放
				return;
			}
			//在会话线程中执行onError操作
			strongSession->async_first([strongSession,err]() {
                //传递强指针是为了确保onError在脱离TCPServer后一定会执行
				strongSession->onError(err);
			});
		});
	}

    //定时管理Session
	void onManagerSession() {
		for (auto &pr : *_sessionMap) {
			weak_ptr<TcpSession> weakSession = pr.second;
			pr.second->async([weakSession]() {
				auto strongSession=weakSession.lock();
				if(!strongSession) {
					return;
				}
				strongSession->onManager();
			});
		}
	}
private:
    Socket::Ptr _socket;
    std::shared_ptr<Timer> _timer;
    std::shared_ptr<SessionMapType > _sessionMap;
    function<TcpSession::Ptr(const std::shared_ptr<ThreadPool> &, const Socket::Ptr &)> _sessionMaker;
};

} /* namespace Network */
} /* namespace ZL */

#endif /* TCPSERVER_TCPSERVER_H */
