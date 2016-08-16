/*
 * recyclePool.h
 *
 *  Created on: 2015年10月29日
 *      Author: root
 */

#ifndef UTIL_RECYCLEPOOL_H_
#define UTIL_RECYCLEPOOL_H_
#include <functional>
#include <mutex>
#include <memory>
#include <atomic>
#include <deque>
namespace ZL {
namespace Util {
using namespace std;

template<typename C, int poolSize = 10>
class recyclePool {
public:
	class poolHelper {
		friend class recyclePool;
	public:
		void quit() {
			*quited = true;
		}
		~poolHelper() {
			if (--(*ref_cout) == 0) {
				delete ref_cout;
				shared_ptr<bool> strongFlag = pool_flag.lock();
				if (strongFlag && !*quited) {
					pool->recycle(ptr);
				} else {
					delete ptr;
				}
				delete quited;
			}
		}
		C *operator->() const noexcept
		{
			return ptr;
		}
		poolHelper(const poolHelper &that) :
				pool(that.pool), pool_flag(that.pool_flag), ptr(that.ptr), ref_cout(
						that.ref_cout), quited(that.quited) {
			++(*ref_cout);
		}
		poolHelper &
		operator=(const poolHelper &that) const noexcept
		{
			pool = that.pool;
			pool_flag = that.pool_flag;
			ref_cout = that.ref_cout;
			ptr = that.ptr;
			quited = that.quited;
			++(*ref_cout);
			return *this;
		}
		C *get() const {
			return ptr;
		}
	private:
		poolHelper(recyclePool *_pool, C* _ptr) :
				pool(_pool), pool_flag(_pool->flag), ptr(_ptr) {
			ref_cout = new atomic_int(1);
			quited = new atomic_bool(false);
		}
		recyclePool *pool;
		weak_ptr<bool> pool_flag;
		C* ptr;
		atomic_int *ref_cout = nullptr;
		atomic_bool *quited;
	};

public:
	typedef poolHelper Helper;
	template<typename ...ArgTypes>
	recyclePool(ArgTypes &&...args) {
		poolsize = poolSize;
		flag = make_shared<bool>(false);
		cteator = [args...]()->C* {
			return new C(args...);
		};
	}
	void setPoolSize(int size) {
		if (size < 0) {
			return;
		}
		poolsize = size;
	}
	void createAll() {
		std::lock_guard<mutex> lck(_mutex);
		for (unsigned int i = 0; i < poolsize; i++) {
			objs.push_back(cteator());
		}
	}
	virtual ~recyclePool() {
		std::lock_guard<mutex> lck(_mutex);
		for (auto &obj : objs) {
			delete obj;
		}
	}
	poolHelper obtain() {
		std::lock_guard<mutex> lck(_mutex);
		if (objs.size() == 0) {
			return poolHelper(this, cteator());
		} else {
			auto obj = objs.front();
			objs.pop_front();
			return poolHelper(this, obj);
		}
	}
	void clear() {
		std::lock_guard<mutex> lck(_mutex);
		for (auto &obj : objs) {
			delete obj;
		}
		objs.clear();
		flag.reset(new bool(false));
	}
	static recyclePool &Instace() {
		static recyclePool pool;
		return pool;
	}

private:
	void recycle(C *obj) {
		std::lock_guard<mutex> lck(_mutex);
		if (objs.size() >= poolsize) {
			delete obj;
			return;
		}
		objs.push_back(obj);
	}
	deque<C*> objs;
	function<C*(void)> cteator;
	shared_ptr<bool> flag;
	std::mutex _mutex;
	unsigned int poolsize;
};
typedef recyclePool<string, 100> PacketPool;
} /* namespace util */
} /* namespace im */

#endif /* UTIL_RECYCLEPOOL_H_ */
