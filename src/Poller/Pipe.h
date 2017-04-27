//
//  Pipe.h
//  xzl
//
//  Created by xzl on 16/4/13.
//

#ifndef Pipe_h
#define Pipe_h

#include <stdio.h>
#include <functional>
#include "EventPoller.h"

using namespace std;

namespace ZL {
namespace Poller {

class Pipe
{
public:
    Pipe(function<void(int size,const char *buf)> &&onRead=nullptr);
    virtual ~Pipe();
    void send(const char *send,int size=0);
private:
    void willWrite(int fd);
    int pipe_fd[2]={-1,-1};
};


}  // namespace Poller
}  // namespace ZL


#endif /* Pipe_h */
