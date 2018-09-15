//
// Created by xzl on 2018/9/13.
//

#ifndef ZLTOOLKIT_TASKEXECUTOR_H
#define ZLTOOLKIT_TASKEXECUTOR_H

#include <memory>
#include <functional>
#include "List.h"
#include "Util/util.h"

using namespace std;
using namespace ZL::Util;


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
            if(size ==0){
                break;
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

class ThreadLoadCounter {
public:
    ThreadLoadCounter(){
        _last_sleep_time = _last_wake_time = getCurrentMicrosecond();
    }
    ~ThreadLoadCounter(){}

    //进入休眠
    void startSleep(){
        _sleeping = true;

        auto current_time = getCurrentMicrosecond();
        auto run_time = current_time - _last_wake_time;
        _last_sleep_time = current_time;

        time_record rcd = {run_time, false};
        _time_list.emplace_back(std::move(rcd));
        countCpuLoad(5 * 1000 * 1000,1024);
    }
    //休眠唤醒
    void sleepWakeUp(){
        _sleeping = false;

        auto current_time = getCurrentMicrosecond();
        auto sleep_time = current_time - _last_sleep_time;
        _last_wake_time = current_time;

        time_record rcd = {sleep_time, true};
        _time_list.emplace_back(std::move(rcd));
        countCpuLoad(5 * 1000 * 1000,1024);
    }

    //cpu负载量
    int load(){
        return _cpu_load;
    }

private:
    inline static uint64_t getCurrentMicrosecond() {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000 * 1000 + tv.tv_usec;
    }

    void countCpuLoad(uint64_t max_time, uint64_t max_size){
        uint64_t totalSleepTime = 0;
        uint64_t totalRunTime = 0;

        _time_list.for_each([&](const time_record& rcd){
            if(rcd.sleep){
                totalSleepTime += rcd.time;
            }else{
                totalRunTime += rcd.time;
            }
        });

        if(_sleeping){
            totalSleepTime += (getCurrentMicrosecond() - _last_sleep_time);
        }else{
            totalRunTime += (getCurrentMicrosecond() - _last_wake_time);
        }

        uint64_t totalTime = totalRunTime + totalSleepTime;

        _cpu_load = totalRunTime * 100 / totalTime;

        while(totalTime > max_time && _time_list.size() != 0 && _time_list.size() > max_size){
            time_record &rcd = _time_list.front();
            totalTime -= rcd.time;
            _time_list.pop_front();
        }
    }

private:
    typedef struct {
        uint64_t time;
        bool sleep;
    } time_record;
private:
    uint64_t _last_sleep_time;
    uint64_t _last_wake_time;
    List<time_record> _time_list;
    bool _sleeping = true;
    int _cpu_load;
};


}//Thread
}//ZL


#endif //ZLTOOLKIT_TASKEXECUTOR_H
