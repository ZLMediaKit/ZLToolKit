/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xia-chu/ZLToolKit).
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

static size_t s_pool_size = 0;

INSTANCE_IMP(WorkThreadPool);

EventPoller::Ptr WorkThreadPool::getFirstPoller(){
    return dynamic_pointer_cast<EventPoller>(_threads.front());
}

EventPoller::Ptr WorkThreadPool::getPoller(){
    return dynamic_pointer_cast<EventPoller>(getExecutor());
}

WorkThreadPool::WorkThreadPool(){
    //最低优先级
    addPoller("work poller", s_pool_size, ThreadPool::PRIORITY_LOWEST, false);
}

void WorkThreadPool::setPoolSize(size_t size) {
    s_pool_size = size;
}

} /* namespace toolkit */

