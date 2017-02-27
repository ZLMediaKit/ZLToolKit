//
//  Pipe.hpp
//  xzl
//
//  Created by xzl on 16/4/13.
//

#ifndef Pipe_hpp
#define Pipe_hpp
#include <functional>
#include <stdio.h>
#include "EventPoller.hpp"
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
    void willWrite(int fd,int event);
    int pipe_fd[2]={-1,-1};
    bool canWrite=true;
    string writeBuf;
};


}  // namespace Poller
}  // namespace ZL


#endif /* Pipe_hpp */
