/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_UTIL_NOTICECENTER_H_
#define SRC_UTIL_NOTICECENTER_H_

#include <mutex>
#include <memory>
#include <string>
#include <exception>
#include <functional>
#include <unordered_map>
#include <thread>
#include "function_traits.h"
#include "onceToken.h"
using namespace std;

namespace toolkit {

class EventDispatcher {
public:
    friend class NoticeCenter;
    typedef std::shared_ptr<EventDispatcher> Ptr;
    ~EventDispatcher() = default;

private:
    typedef unordered_multimap<void *, std::shared_ptr<void> > MapType;
    EventDispatcher() = default;

    class InterruptException : public std::runtime_error {
    public:
        InterruptException() : std::runtime_error("InterruptException") {}
        ~InterruptException() {}
    };

    template<typename ...ArgsType>
    int emitEvent(ArgsType &&...args) {
        typedef function<void(decltype(std::forward<ArgsType>(args))...)> funType;
        decltype(_mapListener) copy;
        {
            //先拷贝(开销比较小)，目的是防止在触发回调时还是上锁状态从而导致交叉互锁
            lock_guard<recursive_mutex> lck(_mtxListener);
            copy = _mapListener;
        }

        int ret = 0;
        for (auto &pr : copy) {
            funType *obj = (funType *) (pr.second.get());
            try {
                (*obj)(std::forward<ArgsType>(args)...);
                ++ret;
            } catch (InterruptException &ex) {
                ++ret;
                break;
            }
        }
        return ret;
    }

    template<typename FUNC>
    void addListener(void *tag, FUNC &&func) {
        typedef typename function_traits<typename std::remove_reference<FUNC>::type>::stl_function_type funType;
        std::shared_ptr<void> pListener(new funType(std::forward<FUNC>(func)), [](void *ptr) {
            funType *obj = (funType *) ptr;
            delete obj;
        });
        lock_guard<recursive_mutex> lck(_mtxListener);
        _mapListener.emplace(tag, pListener);
    }

    void delListener(void *tag, bool &empty) {
        lock_guard<recursive_mutex> lck(_mtxListener);
        _mapListener.erase(tag);
        empty = _mapListener.empty();
    }

private:
    recursive_mutex _mtxListener;
    MapType _mapListener;
};

class NoticeCenter : public std::enable_shared_from_this<NoticeCenter> {
public:
    typedef std::shared_ptr<NoticeCenter> Ptr;
    ~NoticeCenter() {}
    static NoticeCenter &Instance();

    template<typename ...ArgsType>
    int emitEvent(const string &strEvent, ArgsType &&...args) {
        auto dispatcher = getDispatcher(strEvent);
        if (!dispatcher) {
            //该事件无人监听
            return 0;
        }
        return dispatcher->emitEvent(std::forward<ArgsType>(args)...);
    }

    template<typename FUNC>
    void addListener(void *tag, const string &event, FUNC &&func) {
        getDispatcher(event, true)->addListener(tag, std::forward<FUNC>(func));
    }

    void delListener(void *tag, const string &event) {
        auto dispatcher = getDispatcher(event);
        if (!dispatcher) {
            //不存在该事件
            return;
        }
        bool empty;
        dispatcher->delListener(tag, empty);
        if (empty) {
            delDispatcher(event, dispatcher);
        }
    }

    //这个方法性能比较差
    void delListener(void *tag) {
        lock_guard<recursive_mutex> lck(_mtxListener);
        bool empty;
        for (auto it = _mapListener.begin(); it != _mapListener.end();) {
            it->second->delListener(tag, empty);
            if (empty) {
                it = _mapListener.erase(it);
                continue;
            }
            ++it;
        }
    }

    void clearAll() {
        lock_guard<recursive_mutex> lck(_mtxListener);
        _mapListener.clear();
    }
private:
    EventDispatcher::Ptr getDispatcher(const string &event, bool create = false) {
        lock_guard<recursive_mutex> lck(_mtxListener);
        auto it = _mapListener.find(event);
        if (it != _mapListener.end()) {
            return it->second;
        }
        if (create) {
            //如果为空则创建一个
            EventDispatcher::Ptr dispatcher(new EventDispatcher());
            _mapListener.emplace(event, dispatcher);
            return dispatcher;
        }
        return nullptr;
    }

    void delDispatcher(const string &event, const EventDispatcher::Ptr &dispatcher) {
        lock_guard<recursive_mutex> lck(_mtxListener);
        auto it = _mapListener.find(event);
        if (it != _mapListener.end() && dispatcher == it->second) {
            //两者相同则删除
            _mapListener.erase(it);
        }
    }

    NoticeCenter() {}
private:
    recursive_mutex _mtxListener;
    unordered_map<string, EventDispatcher::Ptr> _mapListener;
};

} /* namespace toolkit */
#endif /* SRC_UTIL_NOTICECENTER_H_ */
