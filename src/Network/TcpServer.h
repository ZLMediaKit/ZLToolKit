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

namespace toolkit {

//全局的TcpSession记录对象，方便后面管理
//线程安全的
class SessionMap {
public:
    friend class TcpServer;
    //单例
    static SessionMap &Instance();
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
class TcpServer : public mINI , public std::enable_shared_from_this<TcpServer>{
public:
	typedef std::shared_ptr<TcpServer> Ptr;

    /**
     * 原先的方式是网络事件、数据读取在poller循环中触发，但是处理在后台线程中执行
     * 目前提供两种方式创建TcpServer
     * 1:executorGetter 为WorkThreadPool类型
     * 2:executorGetter 为EventPollerPool类型
     *
     * 方式1：
     * 		此种方式创建的Tcp服务器，所有网络事件会在poller循环中触发，
     * 		但是数据的读取、处理等事件会在后台线程中触发
     * 		所以这种方式跟最初方式相比，poller轮询线程负担更小，网路吞吐量更大
     * 		但是这种方式存在poller线程切换到后台线程的过程，可能性能不是最佳
     *
     * 方式2：
     * 		此种方式创建的Tcp服务器，父文件描述符的accept事件在poller循环中触发
     * 		但是子文件描述符的所有事件、数据读取、数据处理都是在从EventPollerPool中获取的poller线程中执行
     * 		所以这种方式跟最初方式相比，网络事件的触发会派发到多个poller线程中执行
     * 		这种方式网络吞吐量最大
     *
     * 综上，推荐方式2
     * 构造方式参考为 : std::make_shared<TcpServer>(nullptr,nullptr);
     *
     * @param executorGetter 任务执行器获取器
     * @param poller 事件轮询器
     */
    TcpServer(const TaskExecutorGetter::Ptr executorGetter = nullptr,
			  const EventPoller::Ptr &poller = nullptr ) {
		_executorGetter = executorGetter;
		if(!_executorGetter){
			_executorGetter = EventPollerPool::Instance().shared_from_this();
		}
		_poller = poller;
		if(!_poller){
			_poller =  EventPollerPool::Instance().getPoller();
		}

		_executor = _poller;
		_socket = std::make_shared<Socket>(_poller,_executor);
        _socket->setOnAccept(bind(&TcpServer::onAcceptConnection_l, this, placeholders::_1));
		_socket->setOnBeforeAccept(bind(&TcpServer::onBeforeAcceptConnection_l, this));
    }

	~TcpServer() {
		TraceL << "start clean tcp server...";
		_timer.reset();
        //先关闭socket监听，防止收到新的连接
		_socket.reset();

        //务必通知TcpSession已从TcpServer脱离
		for (auto &pr : _sessionMap) {
            //从全局的map中移除记录
            SessionMap::Instance().remove(pr.first);
		}
		_sessionMap.clear();
		TraceL << "clean tcp server completed!";
	}

    //开始监听服务器
    template <typename SessionType>
	void start(uint16_t port, const std::string& host = "0.0.0.0", uint32_t backlog = 1024) {
        //TcpSession创建器，通过它创建不同类型的服务器
        weak_ptr<Socket> weakSock = _socket;
        _sessionMaker = [weakSock](const Socket::Ptr &sock){
			std::shared_ptr<SessionType> ret(new SessionType(nullptr,sock),[weakSock](SessionType *ptr){
				if(!ptr) {
                    return;
                }
                if(!weakSock.lock()){
					//本服务器已经销毁
                    ptr->onError(SockException(Err_other,"Tcp server shutdown!"));
                }
                delete ptr;
			});
			return ret;
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
		},_executor);
		InfoL << "TCP Server listening on " << host << ":" << port;
	}
	 uint16_t getPort(){
		 if(!_socket){
			 return 0;
		 }
		 return _socket->get_local_port();
	 }

protected:
	virtual Socket::Ptr onBeforeAcceptConnection(){
    	//获取任务执行器
    	auto executor = _executorGetter->getExecutor();
    	//该任务执行器可能是ThreadPool也可能是EventPoller
		EventPoller::Ptr poller = dynamic_pointer_cast<EventPoller>(executor);
		if(!poller){
			//如果executor不是EventPoller，那么赋值为TcpServer的poller
			poller = _poller;
		}
		return std::make_shared<Socket>(poller,executor);
    }
    // 接收到客户端连接请求
    virtual void onAcceptConnection(const Socket::Ptr & sock) {
        //创建一个TcpSession;这里实现创建不同的服务会话实例
		auto session = _sessionMaker(sock);
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
        auto success = _sessionMap.emplace(sessionId, session).second;
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
			strongSession->onRecv(buf);
		});

		weak_ptr<TcpServer> weakSelf = shared_from_this();
		//会话接收到错误事件
		sock->setOnErr([weakSelf,weakSession,sessionId](const SockException &err){
		    //在本函数作用域结束时移除会话对象
            //目的是确保移除会话前执行其onError函数
            //同时避免其onError函数抛异常时没有移除会话对象
		    onceToken token(nullptr,[&](){
                //移除掉会话
                SessionMap::Instance().remove(sessionId);
                auto strongSelf = weakSelf.lock();
                if(!strongSelf) {
                    return;
                }
                //在TcpServer对应线程中移除map相关记录
                strongSelf->_executor->async([weakSelf,sessionId](){
                    auto strongSelf = weakSelf.lock();
                    if(!strongSelf){
                        return;
                    }
                    strongSelf->_sessionMap.erase(sessionId);
                });
		    });
			//获取会话强应用
			auto strongSession=weakSession.lock();
            if(strongSession) {
                //触发onError事件回调
				strongSession->onError(err);
			}
		});
	}

    //定时管理Session
	void onManagerSession() {
		for (auto &pr : _sessionMap) {
			weak_ptr<TcpSession> weakSession = pr.second;
			pr.second->async([weakSession]() {
				auto strongSession=weakSession.lock();
				if(!strongSession) {
					return;
				}
				strongSession->onManager();
			}, false);
		}
	}

private:
    Socket::Ptr onBeforeAcceptConnection_l(){
        return onBeforeAcceptConnection();
    }
    // 接收到客户端连接请求
    void onAcceptConnection_l(const Socket::Ptr & sock) {
        onAcceptConnection(sock);
    }
private:
    EventPoller::Ptr _poller;
	TaskExecutor::Ptr _executor;
	TaskExecutorGetter::Ptr _executorGetter;

    Socket::Ptr _socket;
    std::shared_ptr<Timer> _timer;
	unordered_map<string, TcpSession::Ptr > _sessionMap;
    function<TcpSession::Ptr(const Socket::Ptr &)> _sessionMaker;
};

} /* namespace toolkit */

#endif /* TCPSERVER_TCPSERVER_H */
