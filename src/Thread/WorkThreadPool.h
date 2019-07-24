/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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
 
#ifndef UTIL_WORKTHREADPOOL_H_
#define UTIL_WORKTHREADPOOL_H_

#include <memory>
#include "ThreadPool.h"
#include "Poller/EventPoller.h"

using namespace std;

namespace toolkit {

class WorkThreadPool :
		public std::enable_shared_from_this<WorkThreadPool> ,
		public TaskExecutorGetterImp {
public:
	typedef std::shared_ptr<WorkThreadPool> Ptr;
	~WorkThreadPool(){};

	/**
	 * 获取单例
	 * @return
	 */
	static WorkThreadPool &Instance();

	/**
	 * 设置EventPoller个数，在WorkThreadPool单例创建前有效
	 * 在不调用此方法的情况下，默认创建thread::hardware_concurrency()个EventPoller实例
	 * @param size  EventPoller个数，如果为0则为thread::hardware_concurrency()
	 */
	static void setPoolSize(int size = 0);

	/**
	 * 获取第一个实例
	 * @return
	 */
	EventPoller::Ptr getFirstPoller();

	/**
	 * 根据负载情况获取轻负载的实例
	 * 如果优先返回当前线程，那么会返回当前线程
	 * 返回当前线程的目的是为了提高线程安全性
	 * @return
	 */
	EventPoller::Ptr getPoller();
private:
	WorkThreadPool() ;
private:
	static int s_pool_size;
};
} /* namespace toolkit */

#endif /* UTIL_WORKTHREADPOOL_H_ */
