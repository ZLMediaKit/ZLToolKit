//
//  Timer.cpp
//  xzl
//
//  Created by xzl on 16/4/13.
//

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
