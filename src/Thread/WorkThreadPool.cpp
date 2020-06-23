/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "WorkThreadPool.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "ThreadPool.h"

namespace toolkit {

int WorkThreadPool::s_pool_size = 0;

INSTANCE_IMP(WorkThreadPool);

EventPoller::Ptr WorkThreadPool::getFirstPoller(){
    return dynamic_pointer_cast<EventPoller>(_threads.front());
}

EventPoller::Ptr WorkThreadPool::getPoller(){
    return dynamic_pointer_cast<EventPoller>(getExecutor());
}

WorkThreadPool::WorkThreadPool(){
    //创建当前cpu核心个数优先级最低的线程，目的是做些无关紧要的阻塞式任务，例如dns解析，文件io等
    auto size = s_pool_size > 0 ? s_pool_size : thread::hardware_concurrency();
    createThreads([](){
        EventPoller::Ptr ret(new EventPoller(ThreadPool::PRIORITY_LOWEST));
        ret->runLoop(false, false);
        return ret;
    },size);
}

void WorkThreadPool::setPoolSize(int size) {
    s_pool_size = size;
}

} /* namespace toolkit */

