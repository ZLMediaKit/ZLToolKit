/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
	_tag->cancel();
}

}  // namespace toolkit
