//
//  Timer.hpp
//  G平台
//
//  Created by boyo on 16/4/13.
//  Copyright © 2016年 boyo. All rights reserved.
//

#ifndef Timer_hpp
#define Timer_hpp
#include "EventPoller.hpp"
#include "Thread/AsyncTaskThread.h"
#include <stdio.h>
#include <functional>
using namespace std;
using namespace ZL::Thread;

namespace ZL {
namespace Poller {

class Timer{
public:
    Timer(int second,function<bool()> &&cb);
    virtual ~Timer();
};

}  // namespace Poller
}  // namespace ZL
#endif /* Timer_hpp */
