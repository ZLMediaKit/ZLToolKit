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

#include <vector>
#include <iostream>
#include "WorkThreadPool.h"
#include "Util/logger.h"

using namespace ZL::Util;

namespace ZL {
namespace Thread {

WorkThreadPool::WorkThreadPool(int _threadnum) :
		threadnum(_threadnum), threadPos(-1), threads(0) {
	for (int i = 0; i < threadnum; i++) {
		threads.emplace_back(new ThreadPool(1, ThreadPool::PRIORITY_HIGHEST));
	}
}

WorkThreadPool::~WorkThreadPool() {
	InfoL;
	for(auto &thread : threads){
		thread->wait();
	}
}

std::shared_ptr<ThreadPool> &WorkThreadPool::getWorkThread() {
	if (++threadPos >= threadnum) {
		threadPos = 0;
	}
	return threads[threadPos.load()];
}
} /* namespace Thread */
} /* namespace ZL */

