//
//  Timer.cpp
//  xzl
//
//  Created by xzl on 16/4/13.
//

#include "Timer.h"

namespace ZL {
namespace Poller {

Timer::Timer(int second, function<bool()> &&cb) {
	AsyncTaskThread::Instance().DoTaskDelay(reinterpret_cast<uint64_t>(this), second * 1000, [this,cb]()->bool {
		ASYNC_TRACE([this,cb]() {
			if(!cb()) {
				AsyncTaskThread::Instance().CancelTask(reinterpret_cast<uint64_t>(this));
			}
		});
		return true;
	});
}
Timer::~Timer() {
	AsyncTaskThread::Instance().CancelTask(reinterpret_cast<uint64_t>(this));
}

}  // namespace Poller
}  // namespace ZL
