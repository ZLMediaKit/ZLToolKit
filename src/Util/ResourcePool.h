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
#include "Thread/List.h"

using namespace std;

namespace toolkit {

template<typename C>
class ResourcePool {
public:
	typedef std::shared_ptr<C> ValuePtr;
	ResourcePool() {
			pool.reset(new _ResourcePool());
	}
#if (!defined(__GNUC__)) || (__GNUC__ >= 5) || defined(__clang__)
	template<typename ...ArgTypes>
	ResourcePool(ArgTypes &&...args) {
		pool = std::make_shared<_ResourcePool>(std::forward<ArgTypes>(args)...);
	}
#endif //(!defined(__GNUC__)) || (__GNUC__ >= 5) || defined(__clang__)
	void setSize(int size) {
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
			_allotter = []()->C* {
				return new C();
			};
		}
#if (!defined(__GNUC__)) || (__GNUC__ >= 5) || defined(__clang__)
		template<typename ...ArgTypes>
		_ResourcePool(ArgTypes &&...args) {
			_allotter = [args...]()->C* {
				return new C(args...);
			};
		}
#endif //(!defined(__GNUC__)) || (__GNUC__ >= 5) || defined(__clang__)
		~_ResourcePool(){
			std::lock_guard<decltype(_mutex)> lck(_mutex);
//			for(auto &ptr : _objs){
//				delete ptr;
//			}
			_objs.for_each([](C *ptr){
				delete ptr;
			});
		}
		void setSize(int size) {
			_poolsize = size;
		}
		ValuePtr obtain() {
			std::lock_guard<decltype(_mutex)> lck(_mutex);
			C *ptr = nullptr;
			if (_objs.size() == 0) {
				ptr = _allotter();
			} else {
				ptr = _objs.front();
				_objs.pop_front();
			}
			return ValuePtr(ptr, Deleter(this->shared_from_this()));
		}
		void quit(const ValuePtr &ptr) {
			std::lock_guard<decltype(_mutex)> lck(_mutex);
			_quitSet.emplace(ptr.get());
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
			auto it = _quitSet.find(obj);
			if (it != _quitSet.end()) {
				delete obj;
				_quitSet.erase(it);
				return;
			}
			if ((int)_objs.size() >= _poolsize) {
				delete obj;
				return;
			}
			_objs.emplace_back(obj);
		}
    private:
		List<C*> _objs;
		unordered_set<C*> _quitSet;
		function<C*(void)> _allotter;
		mutex _mutex;
		int _poolsize = 8;
	};
	std::shared_ptr<_ResourcePool> pool;
};

} /* namespace toolkit */

#endif /* UTIL_RECYCLEPOOL_H_ */
