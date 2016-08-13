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
AsyncTaskThread Timer::asyncThread(1*1000);

Timer::Timer(int second,function<bool()> &&cb){
    asyncThread.DoTaskDelay(reinterpret_cast<uint64_t>(this), second*1000, [this,cb]()->bool{
        EventPoller::Instance().sendAsync([this,cb](){
            if(!cb()){
                asyncThread.CancelTask(reinterpret_cast<uint64_t>(this));
            }
        });
        return true;
    });
}
Timer::~Timer(){
    asyncThread.CancelTask(reinterpret_cast<uint64_t>(this));
}

}  // namespace Poller
}  // namespace ZL
