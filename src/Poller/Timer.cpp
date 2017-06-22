//
//  Timer.cpp
//  xzl
//
//  Created by xzl on 16/4/13.
//

#include "Timer.h"

namespace ZL {
namespace Poller {

Timer::Timer(float second, const function<bool()> &cb):
		AsyncTaskHelper(second * 1000,[this,cb](){
			ASYNC_TRACE([this,cb]() {
				if(!cb()) {
					AsyncTaskThread::Instance().CancelTask(reinterpret_cast<uint64_t>(this));
				}
			});
			return true;
		})
{
}
Timer::~Timer()
{
}

}  // namespace Poller
}  // namespace ZL
