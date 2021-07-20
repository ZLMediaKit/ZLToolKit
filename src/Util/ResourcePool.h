/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xia-chu/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef UTIL_RECYCLEPOOL_H_
#define UTIL_RECYCLEPOOL_H_

#include <mutex>
#include <deque>
#include <memory>
#include <atomic>
#include <functional>
#include <unordered_set>
#include "Util/List.h"
using namespace std;

namespace toolkit {

#if (defined(__GNUC__) && ( __GNUC__ >= 5 || (__GNUC__ >= 4 && __GNUC_MINOR__ >= 9))) || defined(__clang__) || !defined(__GNUC__)
#define SUPPORT_DYNAMIC_TEMPLATE
#endif


template<typename C>
class ResourcePool_l;
template<typename C>
class ResourcePool;

template<typename C>
class shared_ptr_imp : public std::shared_ptr<C> {
public:
    shared_ptr_imp() {}

    /**
     * 构造智能指针
     * @param ptr 裸指针
     * @param weakPool 管理本指针的循环池
     * @param quit 对接是否放弃循环使用
     */
    shared_ptr_imp(C *ptr,
                   const std::weak_ptr<ResourcePool_l<C> > &weakPool,
                   std::shared_ptr<atomic_bool> quit,
                   const function<void(C *)> &on_recycle);

    /**
     * 放弃或恢复回到循环池继续使用
     * @param flag
     */
    void quit(bool flag = true){
        if(_quit){
            *_quit = flag;
        }
    }
private:
    std::shared_ptr<atomic_bool> _quit;
};

template<typename C>
class ResourcePool_l: public enable_shared_from_this<ResourcePool_l<C> > {
public:
    typedef shared_ptr_imp<C> ValuePtr;
    friend class shared_ptr_imp<C>;
    friend class ResourcePool<C>;

    ResourcePool_l() {
        _allotter = []()->C* {
            return new C();
        };
    }

#if defined(SUPPORT_DYNAMIC_TEMPLATE)
    template<typename ...ArgTypes>
    ResourcePool_l(ArgTypes &&...args) {
        _allotter = [args...]()->C* {
            return new C(args...);
        };
    }
#endif //defined(SUPPORT_DYNAMIC_TEMPLATE)

    ~ResourcePool_l(){
        _objs.for_each([](C *ptr){
            delete ptr;
        });
    }

    void setSize(size_t size) {
        _poolsize = size;
    }

    ValuePtr obtain(const function<void(C *)> &on_recycle = nullptr) {
        C *ptr;
        auto is_busy = _busy.test_and_set();
        if (!is_busy) {
            //获取到锁
            if (_objs.size() == 0) {
                ptr = _allotter();
            } else {
                ptr = _objs.front();
                _objs.pop_front();
            }
            _busy.clear();
        } else {
            //未获取到锁
            ptr = _allotter();
        }
        return ValuePtr(ptr, _weak_self, std::make_shared<atomic_bool>(false), on_recycle);
    }

private:
    void recycle(C *obj) {
        auto is_busy = _busy.test_and_set();
        if (!is_busy) {
            //获取到锁
            if (_objs.size() >= _poolsize) {
                delete obj;
            } else {
                _objs.emplace_back(obj);
            }
            _busy.clear();
        } else {
            //未获取到锁
            delete obj;
        }
    }

    void setup(){
        _weak_self = this->shared_from_this();
    }

private:
    size_t _poolsize = 8;
    List<C*> _objs;
    function<C*(void)> _allotter;
    atomic_flag _busy{false};
    weak_ptr<ResourcePool_l > _weak_self;
};

/**
 * 循环池，注意，循环池里面的对象不能继承enable_shared_from_this！
 * @tparam C
 */
template<typename C>
class ResourcePool {
public:
    typedef shared_ptr_imp<C> ValuePtr;
    ResourcePool() {
        pool.reset(new ResourcePool_l<C>());
        pool->setup();
    }
#if defined(SUPPORT_DYNAMIC_TEMPLATE)
    template<typename ...ArgTypes>
    ResourcePool(ArgTypes &&...args) {
        pool = std::make_shared<ResourcePool_l<C> >(std::forward<ArgTypes>(args)...);
        pool->setup();
    }
#endif //defined(SUPPORT_DYNAMIC_TEMPLATE)
    void setSize(size_t size) {
        pool->setSize(size);
    }
    ValuePtr obtain(const function<void(C *)> &on_recycle = nullptr) {
        return pool->obtain(on_recycle);
    }
private:
    std::shared_ptr<ResourcePool_l<C> > pool;
};

template<typename C>
shared_ptr_imp<C>::shared_ptr_imp(C *ptr,
                                  const std::weak_ptr<ResourcePool_l<C> > &weakPool,
                                  std::shared_ptr<atomic_bool> quit,
                                  const function<void(C *)> &on_recycle) :
        shared_ptr<C>(ptr, [weakPool, quit, on_recycle](C *ptr) {
            if (on_recycle) {
                on_recycle(ptr);
            }
            auto strongPool = weakPool.lock();
            if (strongPool && !(*quit)) {
                //循环池还在并且不放弃放入循环池
                strongPool->recycle(ptr);
            } else {
                delete ptr;
            }
        }), _quit(std::move(quit)) {}

} /* namespace toolkit */
#endif /* UTIL_RECYCLEPOOL_H_ */
