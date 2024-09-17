/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TASKQUEUE_H_
#define TASKQUEUE_H_

#include <mutex>
#include "Util/List.h"
#include "semaphore.h"

namespace toolkit {

//实现了一个基于函数对象的任务列队，该列队是线程安全的，任务列队任务数由信号量控制  [AUTO-TRANSLATED:67e02e93]
//Implemented a task queue based on function objects, which is thread-safe, and the number of tasks in the task queue is controlled by a semaphore
template<typename T>
class TaskQueue {
public:
    //打入任务至列队  [AUTO-TRANSLATED:d08b5817]
    //Put a task into the queue
    template<typename C>
    void push_task(C &&task_func) {
        {
            std::lock_guard<decltype(_mutex)> lock(_mutex);
            _queue.emplace_back(std::forward<C>(task_func));
        }
        _sem.post();
    }

    template<typename C>
    void push_task_first(C &&task_func) {
        {
            std::lock_guard<decltype(_mutex)> lock(_mutex);
            _queue.emplace_front(std::forward<C>(task_func));
        }
        _sem.post();
    }

    //清空任务列队  [AUTO-TRANSLATED:dbcd7fe9]
    //Clear the task queue
    void push_exit(size_t n) {
        _sem.post(n);
    }

    //从列队获取一个任务，由执行线程执行  [AUTO-TRANSLATED:4a1143ae]
    //Get a task from the queue and execute it by the executing thread
    bool get_task(T &tsk) {
        _sem.wait();
        std::lock_guard<decltype(_mutex)> lock(_mutex);
        if (_queue.empty()) {
            return false;
        }
        tsk = std::move(_queue.front());
        _queue.pop_front();
        return true;
    }

    size_t size() const {
        std::lock_guard<decltype(_mutex)> lock(_mutex);
        return _queue.size();
    }

private:
    List <T> _queue;
    mutable std::mutex _mutex;
    semaphore _sem;
};

} /* namespace toolkit */
#endif /* TASKQUEUE_H_ */
