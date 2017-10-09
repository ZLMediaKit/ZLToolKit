/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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

namespace ZL {
namespace Poller {

Timer::Timer(float second, const function<bool()> &cb)
{
	canceled.reset(new bool);
	std::weak_ptr<bool> canceledWeak = canceled;
	AsyncTaskThread::Instance().DoTaskDelay(reinterpret_cast<uint64_t>(this),second * 1000,[this,cb,canceledWeak](){
		ASYNC_TRACE([this,cb,canceledWeak]() {
			auto canceledStrog = canceledWeak.lock();
			if(!canceledStrog){
				AsyncTaskThread::Instance().CancelTask(reinterpret_cast<uint64_t>(this));
				return;
			}
			if(!cb()) {
				AsyncTaskThread::Instance().CancelTask(reinterpret_cast<uint64_t>(this));
			}
		});
		return true;
	});


}
Timer::~Timer()
{
	AsyncTaskThread::Instance().CancelTask(reinterpret_cast<uint64_t>(this));
}

}  // namespace Poller
}  // namespace ZL
