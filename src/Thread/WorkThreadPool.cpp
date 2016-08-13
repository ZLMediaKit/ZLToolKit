/*
 * WorkThreadPool.cpp
 *
 *  Created on: 2015年10月30日
 *      Author: root
 */

#include "WorkThreadPool.h"
#include <vector>
#include <iostream>
namespace ZL {
namespace Thread {

WorkThreadPool::WorkThreadPool(int _threadnum) :
		threadnum(_threadnum), threadPos(-1), threads(0) {
	for (int i = 0; i < threadnum; i++) {
		threads.emplace_back(new ThreadPool(1, ThreadPool::PRIORITY_HIGHEST));
	}
}

WorkThreadPool::~WorkThreadPool() {

}
void WorkThreadPool::wait() {
	for (auto &th : threads) {
		th->wait();
	}
}

void WorkThreadPool::post(int &pos, const Task &task) {
	if (pos < 0 || pos >= threadnum) {
		if (++threadPos >= threadnum) {
			threadPos = 0;
		}
		pos = threadPos.load();
	}
	threads[pos]->post(task);
}

shared_ptr<ThreadPool> &WorkThreadPool::getWorkThread() {
	if (++threadPos >= threadnum) {
		threadPos = 0;
	}
	return threads[threadPos.load()];
}
} /* namespace Thread */
} /* namespace ZL */

