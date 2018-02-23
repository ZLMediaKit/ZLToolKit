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

#ifndef SERVER_SESSION_H_
#define SERVER_SESSION_H_
#include <memory>
#include <sstream>
#include "Socket.h"
#include "Util/logger.h"
#include "Util/mini.h"
#include "Thread/ThreadPool.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Thread;

namespace ZL {
namespace Network {

class TcpSession: public std::enable_shared_from_this<TcpSession> , public SocketHelper {
public:
    typedef std::shared_ptr<TcpSession> Ptr;

	TcpSession(const std::shared_ptr<ThreadPool> &th, const Socket::Ptr &sock);
	virtual ~TcpSession();
    //接收数据入口
	virtual void onRecv(const Buffer::Ptr &) = 0;
    //收到eof或其他导致脱离TcpServer事件的回调
	virtual void onError(const SockException &err) = 0;
    //每隔一段时间触发，用来做超时管理
	virtual void onManager() =0;
    //在创建TcpSession后，TcpServer会把自身的配置参数通过该函数传递给TcpSession
    virtual void attachServer(const mINI &ini){};
    //作为该TcpSession的唯一标识符
    virtual string getIdentifier() const;
    //在TcpSession绑定的线程中异步排队执行任务
	template <typename T>
	void async(T &&task) {
		_th->async(std::forward<T>(task));
	}
    //在TcpSession绑定的线程中最高优先级异步执行任务
    template <typename T>
	void async_first(T &&task) {
		_th->async_first(std::forward<T>(task));
	}
    //安全的脱离TcpServer并触发onError事件
    void safeShutdown();
private:
    std::shared_ptr<ThreadPool> _th;
};


} /* namespace Session */
} /* namespace ZL */

#endif /* SERVER_SESSION_H_ */
