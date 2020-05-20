/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef THREADGROUP_H_
#define THREADGROUP_H_

#include <set>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_map>
using namespace std;

namespace toolkit {

class thread_group {
private:
    thread_group(thread_group const&);
    thread_group& operator=(thread_group const&);
public:
    thread_group() {
    }
    ~thread_group() {
        _threads.clear();
    }

    bool is_this_thread_in() {
        auto thread_id = this_thread::get_id();
        if(_thread_id == thread_id){
            return true;
        }
        return _threads.find(thread_id) != _threads.end();
    }

    bool is_thread_in(thread* thrd) {
        if (!thrd) {
            return false;
        }
        auto it = _threads.find(thrd->get_id());
        return it != _threads.end();
    }

    template<typename F>
    thread* create_thread(F &&threadfunc) {
        auto thread_new = std::make_shared<thread>(threadfunc);
        _thread_id = thread_new->get_id();
        _threads[_thread_id] = thread_new;
        return thread_new.get();
    }

    void remove_thread(thread* thrd) {
        auto it = _threads.find(thrd->get_id());
        if (it != _threads.end()) {
            _threads.erase(it);
        }
    }
    void join_all() {
        if (is_this_thread_in()) {
            throw runtime_error("thread_group: trying joining itself");
        }
        for (auto &it : _threads) {
            if (it.second->joinable()) {
                it.second->join(); //等待线程主动退出
            }
        }
        _threads.clear();
    }
    size_t size() {
        return _threads.size();
    }
private:
    unordered_map<thread::id, std::shared_ptr<thread> > _threads;
    thread::id _thread_id;
};

} /* namespace toolkit */
#endif /* THREADGROUP_H_ */
