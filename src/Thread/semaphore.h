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

#ifndef SEMAPHORE_H_
#define SEMAPHORE_H_

/*
 * 目前发现信号量在32位的系统上有问题，
 * 休眠的线程无法被正常唤醒，先禁用之
 #if defined(__linux__)
#include <semaphore.h>
#define HAVE_SEM
#endif //HAVE_SEM
*/
#include <atomic>
#include <mutex>
#include <condition_variable>
using namespace std;

namespace ZL {
namespace Thread {

class semaphore {
public:
	explicit semaphore(unsigned int initial = 0) {
#if defined(HAVE_SEM)
		sem_init(&_sem, 0, initial);
#else
		_count = 0;
#endif
	}
	~semaphore() {
#if defined(HAVE_SEM)
		sem_destroy(&_sem);
#endif
	}
	void post(unsigned int n = 1) {
#if defined(HAVE_SEM)
		while (n--) {
			sem_post(&_sem);
		}
#else
		_count += n;
		while (n--) {
			_condition.notify_one();
		}
#endif

	}
	void wait() {
#if defined(HAVE_SEM)
		sem_wait(&_sem);
#else
        while (_count == 0) {
			unique_lock<mutex> lock(_mutex);
			_condition.wait(lock);
		}
		--_count;
#endif
	}
private:
#if defined(HAVE_SEM)
	sem_t _sem;
#else
	atomic_int _count;
	mutex _mutex;
	condition_variable_any _condition;
#endif
};
} /* namespace Thread */
} /* namespace ZL */
#endif /* SEMAPHORE_H_ */
