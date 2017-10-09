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

#ifndef UTIL_RWMUTEX_H_
#define UTIL_RWMUTEX_H_

#include <mutex>
#include <atomic>

using namespace std;

namespace ZL {
namespace Thread {

class rw_mutex {
public:
	rw_mutex() :
			reader_cnt(0) {
	}
	void lock(bool write_mode = true) {
		if (write_mode) {
			//write thread
			mtx_write.lock();
		} else {
			// read thread
			lock_guard<mutex> lck(mtx_reader);
			if (reader_cnt++ == 0) {
				mtx_write.lock();
			}
		}
	}
	void unlock(bool write_mode = true) {
		if (write_mode) {
			//write thread
			mtx_write.unlock();
		} else {
			// read thread
			lock_guard<mutex> lck(mtx_reader);
			if (--reader_cnt == 0) {
				mtx_write.unlock();
			}
		}
	}
	virtual ~rw_mutex() {
	}
private:
	mutex mtx_write;
	mutex mtx_reader;
	int reader_cnt;
};
class lock_guard_rw {
public:
	lock_guard_rw(rw_mutex &_mtx, bool _write_mode = true) :
			mtx(_mtx), write_mode(_write_mode) {
		mtx.lock(write_mode);
	}
	virtual ~lock_guard_rw() {
		mtx.unlock(write_mode);
	}
private:
	rw_mutex &mtx;
	bool write_mode;
};
} /* namespace Thread */
} /* namespace ZL */

#endif /* UTIL_RWMUTEX_H_ */
