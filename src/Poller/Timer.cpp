/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Timer.h"

namespace toolkit {

Timer::Timer(float second, const std::function<bool()> &cb, const EventPoller::Ptr &poller) {
    _poller = poller;
    if (!_poller) {
        _poller = EventPollerPool::Instance().getPoller();
    }
    _tag = _poller->doDelayTask((uint64_t) (second * 1000), [cb, second]() {
        try {
            if (cb()) {
                //重复的任务  [AUTO-TRANSLATED:2d440b54]
                //Recurring task
                return (uint64_t) (1000 * second);
            }
            //该任务不再重复  [AUTO-TRANSLATED:4249fc53]
            //This task no longer recurs
            return (uint64_t) 0;
        } catch (std::exception &ex) {
            ErrorL << "Exception occurred when do timer task: " << ex.what();
            return (uint64_t) (1000 * second);
        }
    });
}

Timer::~Timer() {
    auto tag = _tag.lock();
    if (tag) {
        tag->cancel();
    }
}

}  // namespace toolkit
