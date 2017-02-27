//
//  Timer.cpp
//  G平台
//
//  Created by boyo on 16/4/13.
//  Copyright © 2016年 boyo. All rights reserved.
//

#include "Timer.hpp"

namespace ZL {
namespace Poller {

Timer::Timer(int second, function<bool()> &&cb) {
	AsyncTaskThread::Instance().DoTaskDelay(reinterpret_cast<uint64_t>(this), second * 1000, [this,cb]()->bool {
		EventPoller::Instance().async([this,cb]() {
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
