/*
 * WorkThreadPool.cpp
 *
 *  Created on: 2015年10月30日
 *      Author: root
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
	wait();
	InfoL;
}
void WorkThreadPool::wait() {
	for (auto &th : threads) {
		th->wait();
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

