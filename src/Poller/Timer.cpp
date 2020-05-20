/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Timer.h"

namespace toolkit {

Timer::Timer(float second,
             const function<bool()> &cb,
             const EventPoller::Ptr &poller,
             bool continueWhenException) {
    _poller = poller;
    if(!_poller){
        _poller = EventPollerPool::Instance().getPoller();
    }
    _tag = _poller->doDelayTask(second * 1000, [cb, second , continueWhenException]() {
        try {
            if (cb()) {
                //重复的任务
                return (uint64_t) (1000 * second);
            }
            //该任务不再重复
            return (uint64_t) 0;
        }catch (std::exception &ex){
            ErrorL << "执行定时器任务捕获到异常:" << ex.what();
            return continueWhenException ? (uint64_t) (1000 * second) : 0;
        }
    });
}

Timer::~Timer() {
    auto tag = _tag.lock();
    if(tag){
        tag->cancel();
    }
}

}  // namespace toolkit
