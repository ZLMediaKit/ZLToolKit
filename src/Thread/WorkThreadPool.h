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
 
#ifndef UTIL_WORKTHREADPOOL_H_
#define UTIL_WORKTHREADPOOL_H_

#include <map>
#include <thread>
#include <memory>
#include <atomic>
#include <iostream>
#include <unordered_map>
#include "ThreadPool.h"

using namespace std;

namespace toolkit {

class WorkThreadPool :
        public std::enable_shared_from_this<WorkThreadPool> ,
        public TaskExecutorGetterImp{
public:
	typedef std::shared_ptr<WorkThreadPool> Ptr;
    ~WorkThreadPool(){};
    static WorkThreadPool &Instance();
    static void Destory();
private:
    WorkThreadPool();
};

} /* namespace toolkit */

#endif /* UTIL_WORKTHREADPOOL_H_ */
