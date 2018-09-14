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
    //等待线程退出
    virtual void wait() = 0;
    virtual void shutdown() = 0;
    //任务个数
    virtual uint64_t size() = 0;
};

class TaskExecutorGetter {
public:
    typedef shared_ptr<TaskExecutorGetter> Ptr;
    virtual ~TaskExecutorGetter(){};
    virtual TaskExecutor::Ptr getExecutor() = 0;
    virtual void wait() = 0;
    virtual void shutdown() = 0;
};


class TaskExecutorGetterImp : public TaskExecutorGetter{
public:
    template <typename FUN>
    TaskExecutorGetterImp(FUN &&fun,int threadnum = thread::hardware_concurrency()){
        _threadnum = threadnum;
        for (int i = 0; i < _threadnum; i++) {
            _threads.emplace_back(fun());
        }
    }

    ~TaskExecutorGetterImp(){
        shutdown();
        wait();
        _threads.clear();
    }

    TaskExecutor::Ptr getExecutor() override{
        TaskExecutor::Ptr ret = _threads.front();
        auto minSize = ret->size();
        //获取任务数最少的线程
        for(auto &th : _threads){
            auto size = th->size();
            if(size < minSize){
                minSize = size;
                ret = th;
            }
        }
        return ret;
    }

    void wait() override{
        for (auto &th : _threads){
            th->wait();
        }
    }
    void shutdown() override{
        for (auto &th : _threads){
            th->shutdown();
        }
    }
protected:
    int _threadnum;
    vector <TaskExecutor::Ptr > _threads;
};

}//Thread
}//ZL


#endif //ZLTOOLKIT_TASKEXECUTOR_H
