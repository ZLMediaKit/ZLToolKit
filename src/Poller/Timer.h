//
//  Timer.h
//  xzl
//
//  Created by xzl on 16/4/13.
//

#ifndef Timer_h
#define Timer_h

#include <stdio.h>
#include <functional>
#include "EventPoller.h"
#include "Thread/AsyncTaskThread.h"

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
#endif /* Timer_h */
