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

#ifndef SERVER_LIMITEDSESSION_H_
#define SERVER_LIMITEDSESSION_H_
#include <memory>
#include "Util/logger.h"
#include "TcpSession.h"

using namespace std;
using namespace ZL::Util;

namespace ZL {
namespace Network {

template<int MaxCount>
class TcpLimitedSession: public TcpSession {
public:
	TcpLimitedSession(const std::shared_ptr<ThreadPool> &_th, const Socket::Ptr &_sock) :
		TcpSession(_th,_sock) {
		lock_guard<recursive_mutex> lck(stackMutex());
		static uint64_t maxSeq(0);
		sessionSeq = maxSeq++;
		auto &stack = getStack();
		stack.emplace(this);

		if(stack.size() > MaxCount){
			auto it = stack.begin();
			(*it)->safeShutdown();
			stack.erase(it);
			WarnL << "超过TCP个数限制:" << MaxCount;
		}
	}
	virtual ~TcpLimitedSession() {
		lock_guard<recursive_mutex> lck(stackMutex());
		getStack().erase(this);
	}
private:
	uint64_t sessionSeq; //会话栈顺序
	struct Comparer {
		bool operator()(TcpLimitedSession *x, TcpLimitedSession *y) const {
			return x->sessionSeq < y->sessionSeq;
		}
	};
	static recursive_mutex &stackMutex(){
		static recursive_mutex mtx;
		return mtx;
	}
	//RTSP会话栈,先创建的在前面
	static set<TcpLimitedSession *, Comparer> &getStack(){
		static set<TcpLimitedSession *, Comparer> stack;
		return stack;
	}
};


} /* namespace Session */
} /* namespace ZL */

#endif /* SERVER_LIMITEDSESSION_H_ */
