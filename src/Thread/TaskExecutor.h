//
// Created by xzl on 2018/9/13.
//

#ifndef ZLTOOLKIT_TASKEXECUTOR_H
#define ZLTOOLKIT_TASKEXECUTOR_H

#include <memory>
#include <functional>
using namespace std;

namespace ZL {
namespace Thread {

class TaskExecutor {
public:
    typedef function<void()> Task;
    typedef shared_ptr<TaskExecutor> Ptr;

    virtual ~TaskExecutor(){}
    //把任务打入线程池并异步执行
    virtual bool async(const Task &task, bool may_sync = true) = 0;
    virtual bool async_first(const Task &task, bool may_sync = true) {
        return async(task,may_sync);
    };
    virtual bool sync(const Task &task) = 0;
    virtual bool sync_first(const Task &task) {
        return sync(task);
    };
};

class TaskExecutorGetter {
public:
    typedef shared_ptr<TaskExecutorGetter> Ptr;
    virtual TaskExecutor::Ptr getExecutor() = 0;
};

}//Thread
}//ZL


#endif //ZLTOOLKIT_TASKEXECUTOR_H
