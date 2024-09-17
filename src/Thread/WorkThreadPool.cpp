/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "WorkThreadPool.h"

namespace toolkit {

static size_t s_pool_size = 0;
static bool s_enable_cpu_affinity = true;

INSTANCE_IMP(WorkThreadPool)

EventPoller::Ptr WorkThreadPool::getFirstPoller() {
    return std::static_pointer_cast<EventPoller>(_threads.front());
}

EventPoller::Ptr WorkThreadPool::getPoller() {
    return std::static_pointer_cast<EventPoller>(getExecutor());
}

WorkThreadPool::WorkThreadPool() {
    //最低优先级  [AUTO-TRANSLATED:cd1f0dbc]
    //Lowest priority
    addPoller("work poller", s_pool_size, ThreadPool::PRIORITY_LOWEST, false, s_enable_cpu_affinity);
}

void WorkThreadPool::setPoolSize(size_t size) {
    s_pool_size = size;
}

void WorkThreadPool::enableCpuAffinity(bool enable) {
    s_enable_cpu_affinity = enable;
}

} /* namespace toolkit */

