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

#ifndef UTIL_RECYCLEPOOL_H_
#define UTIL_RECYCLEPOOL_H_

#include <mutex>
#include <deque>
#include <memory>
#include <atomic>
#include <functional>
#include <unordered_set>
#include "Thread/spin_mutex.h"
#include "Thread/List.h"

using namespace ZL::Thread;

namespace ZL {
namespace Util {
using namespace std;

template<typename C, int poolSize = 10>
class ResourcePool {
public:
	typedef std::shared_ptr<C> ValuePtr;
	ResourcePool() {
			pool.reset(new _ResourcePool());
	}
#if (!defined(__GNUC__)) || (__GNUC__ >= 5) || defined(__clang__)
	template<typename ...ArgTypes>
	ResourcePool(ArgTypes &&...args) {
		pool.reset(new _ResourcePool(std::forward<ArgTypes>(args)...));
	}
#endif //(!defined(__GNUC__)) || (__GNUC__ >= 5) || defined(__clang__)
	void reSize(int size) {
		pool->setSize(size);
	}
	ValuePtr obtain() {
		return pool->obtain();
	}
	void quit(const ValuePtr &ptr) {
		pool->quit(ptr);
	}
private:

	class _ResourcePool: public enable_shared_from_this<_ResourcePool> {
	public:
		typedef std::shared_ptr<C> ValuePtr;
		_ResourcePool() {
			poolsize = poolSize;
			allotter = []()->C* {
				return new C();
			};
		}
#if (!defined(__GNUC__)) || (__GNUC__ >= 5) || defined(__clang__)
		template<typename ...ArgTypes>
		_ResourcePool(ArgTypes &&...args) {
			poolsize = poolSize;
			allotter = [args...]()->C* {
				return new C(args...);
			};
		}
#endif //(!defined(__GNUC__)) || (__GNUC__ >= 5) || defined(__clang__)
		virtual ~_ResourcePool(){
			std::lock_guard<decltype(_mutex)> lck(_mutex);
//			for(auto &ptr : objs){
//				delete ptr;
//			}
			objs.for_each([](C *ptr){
				delete ptr;
			});
		}
		void setSize(int size) {
			poolsize = size;
		}
		ValuePtr obtain() {
			std::lock_guard<decltype(_mutex)> lck(_mutex);
			C *ptr = nullptr;
			if (objs.size() == 0) {
				ptr = allotter();
			} else {
				ptr = objs.front();
				objs.pop_front();
			}
			return ValuePtr(ptr, Deleter(this->shared_from_this()));
		}
		void quit(const ValuePtr &ptr) {
			std::lock_guard<decltype(_mutex)> lck(_mutex);
			quitSet.emplace(ptr.get());
		}
	private:
		class Deleter {
		public:
			Deleter(const std::shared_ptr<_ResourcePool> &pool) {
				weakPool = pool;
			}
			void operator()(C *ptr) {
				auto strongPool = weakPool.lock();
				if (strongPool) {
					strongPool->recycle(ptr);
				} else {
					delete ptr;
				}
			}
		private:
			weak_ptr<_ResourcePool> weakPool;
		};
	private:
		void recycle(C *obj) {
			std::lock_guard<decltype(_mutex)> lck(_mutex);
			auto it = quitSet.find(obj);
			if (it != quitSet.end()) {
				delete obj;
				quitSet.erase(it);
				return;
			}
			if ((int)objs.size() >= poolsize) {
				delete obj;
				return;
			}
			objs.emplace_back(obj);
		}

		List<C*> objs;
		unordered_set<C*> quitSet;
		function<C*(void)> allotter;
		spin_mutex _mutex;
		int poolsize;
	};
	std::shared_ptr<_ResourcePool> pool;
};

} /* namespace util */
} /* namespace im */

#endif /* UTIL_RECYCLEPOOL_H_ */
