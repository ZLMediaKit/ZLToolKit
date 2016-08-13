/*
 * WorkThreadPool.h
 *
 *  Created on: 2015年10月30日
 *      Author: root
 */

#ifndef UTIL_WORKTHREADPOOL_H_
#define UTIL_WORKTHREADPOOL_H_
#include "ThreadPool.hpp"
#include <unordered_map>
#include <map>
#include <thread>
#include <memory>
#include <atomic>
#include <iostream>
using namespace std;

namespace ZL {
namespace Thread {

class WorkThreadPool {
public:
	WorkThreadPool(int threadnum = thread::hardware_concurrency());
	virtual ~WorkThreadPool();
	void post(int &pos, const Task &task);
	shared_ptr<ThreadPool> &getWorkThread();
	static WorkThreadPool &Instance() {
		static WorkThreadPool intance;
		return intance;
	}
	void wait();
private:
	int threadnum;
	atomic<int> threadPos;
	vector<shared_ptr<ThreadPool> > threads;
};

} /* namespace Thread */
} /* namespace ZL */

#endif /* UTIL_WORKTHREADPOOL_H_ */
