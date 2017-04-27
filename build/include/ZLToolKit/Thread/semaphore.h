/*
 * semaphore.h
 *
 *  Created on: 2015年8月14日
 *      Author: root
 */

#ifndef SEMAPHORE_H_
#define SEMAPHORE_H_

#ifdef  __linux__
#include <semaphore.h>
#define HAVE_SEM
#else
#include <mutex>
#include <atomic>
#include <condition_variable>
using namespace std;
#endif //HAVE_SEM


namespace ZL {
namespace Thread {

class semaphore {
public:
	explicit semaphore(unsigned int initial = 0) {
#ifdef HAVE_SEM
		sem_init(&sem, 0, initial);
#else
		count_ = 0;
#endif
	}
	~semaphore() {
#ifdef HAVE_SEM
		sem_destroy(&sem);
#endif
	}
	void post(unsigned int n = 1) {
#ifdef HAVE_SEM
		while (n--) {
			sem_post(&sem);
		}
#else
		unique_lock<mutex> lock(mutex_);
		count_ += n;
		while (n--) {
			condition_.notify_one();
		}
#endif

	}
	void wait() {
#ifdef HAVE_SEM
		sem_wait(&sem);
#else
		unique_lock<mutex> lock(mutex_);
		while (count_ == 0) {
			condition_.wait(lock);
		}
		--count_;
#endif
	}
private:
#ifdef HAVE_SEM
	sem_t sem;
#else
	atomic_uint count_;
	mutex mutex_;
	condition_variable_any condition_;
#endif
};
} /* namespace Thread */
} /* namespace ZL */
#endif /* SEMAPHORE_H_ */
