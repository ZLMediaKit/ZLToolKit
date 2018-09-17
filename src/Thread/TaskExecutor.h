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

/**
 * cpu负载计算器
 */
class ThreadLoadCounter {
public:
    /**
     * 构造函数
     * @param max_size 统计样本数量
     * @param max_usec 统计时间窗口,亦即最近{max_usec}的cpu负载率
     */
    ThreadLoadCounter(uint64_t max_size,uint64_t max_usec){
        _last_sleep_time = _last_wake_time = getCurrentMicrosecond();
        _max_size = max_size;
        _max_usec = max_usec;
    }
    ~ThreadLoadCounter(){}

    //进入休眠
    void startSleep(){
        lock_guard<mutex> lck(_mtx);

        _sleeping = true;

        auto current_time = getCurrentMicrosecond();
        auto run_time = current_time - _last_wake_time;
        _last_sleep_time = current_time;

        _time_list.emplace_back(run_time, false);

        if(_time_list.size() > _max_size){
            _time_list.pop_front();
        }
    }
    //休眠唤醒
    void sleepWakeUp(){
        lock_guard<mutex> lck(_mtx);

        _sleeping = false;

        auto current_time = getCurrentMicrosecond();
        auto sleep_time = current_time - _last_sleep_time;
        _last_wake_time = current_time;

        _time_list.emplace_back(sleep_time, true);

        if(_time_list.size() > _max_size){
            _time_list.pop_front();
        }
    }

    //返回当前线程cpu使用率，范围为 0 ~ 100
    int load(){
        lock_guard<mutex> lck(_mtx);

        uint64_t totalSleepTime = 0;
        uint64_t totalRunTime = 0;

        _time_list.for_each([&](const TimeRecord& rcd){
            if(rcd._sleep){
                totalSleepTime += rcd._time;
            }else{
                totalRunTime += rcd._time;
            }
        });

        if(_sleeping){
            totalSleepTime += (getCurrentMicrosecond() - _last_sleep_time);
        }else{
            totalRunTime += (getCurrentMicrosecond() - _last_wake_time);
        }

        uint64_t totalTime = totalRunTime + totalSleepTime;

        while((_time_list.size() != 0) && (totalTime > _max_usec || _time_list.size() > _max_size)){
            TimeRecord &rcd = _time_list.front();
            if(rcd._sleep){
                totalSleepTime -= rcd._time;
            }else{
                totalRunTime -= rcd._time;
            }
            totalTime -= rcd._time;
            _time_list.pop_front();
        }
        return totalRunTime * 100 / totalTime;
    }

private:
    inline static uint64_t getCurrentMicrosecond() {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000 * 1000 + tv.tv_usec;
    }
private:
    class TimeRecord {
    public:
        TimeRecord(uint64_t tm,bool slp){
            _time = tm;
            _sleep = slp;
        }
    public:
        uint64_t _time;
        bool _sleep;
    };
private:
    uint64_t _last_sleep_time;
    uint64_t _last_wake_time;
    List<TimeRecord> _time_list;
    bool _sleeping = true;
    uint64_t _max_size;
    uint64_t _max_usec;
    mutex _mtx;
};

class TaskExecutor : public ThreadLoadCounter{
public:
    typedef function<void()> Task;
    typedef shared_ptr<TaskExecutor> Ptr;

    TaskExecutor(uint64_t max_size = 32,uint64_t max_usec = 2 * 1000 * 1000):ThreadLoadCounter(max_size,max_usec){}
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
        for (int i = 0; i < threadnum; i++) {
            _threads.emplace_back(fun());
        }
    }

    ~TaskExecutorGetterImp(){
        shutdown();
        wait();
        _threads.clear();
    }

    TaskExecutor::Ptr getExecutor() override{
        auto thread_pos = _thread_pos;
        if(thread_pos >= _threads.size()){
            thread_pos = _threads.size() - 1;
        }

        TaskExecutor::Ptr executor_min_load = _threads[thread_pos];
        auto min_load = executor_min_load->load();

        for(int i = 0; i < _threads.size() && min_load != 0; ++i , ++thread_pos ){
            if(thread_pos >= _threads.size()){
                thread_pos = _threads.size() - 1;
            }

            auto th = _threads[thread_pos];
            auto load = th->load();

            if(load < min_load){
                min_load = load;
                executor_min_load = th;
            }
        }
        _thread_pos = thread_pos;
        return executor_min_load;
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
    vector<int> getExecutorLoad(){
        vector<int> vec(_threads.size());
        int i = 0;
        for(auto &executor : _threads){
            vec[i++] = executor->load();
        }
        return vec;
    }
protected:
    vector <TaskExecutor::Ptr > _threads;
    int _thread_pos = 0;
};

}//Thread
}//ZL


#endif //ZLTOOLKIT_TASKEXECUTOR_H
