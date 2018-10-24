//
// Created by xzl on 2018/9/13.
//

#ifndef ZLTOOLKIT_TASKEXECUTOR_H
#define ZLTOOLKIT_TASKEXECUTOR_H

#include <memory>
#include <functional>
#include "List.h"
#include "Util/util.h"
#include "Util/TimeTicker.h"

using namespace std;

namespace toolkit {

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

    /**
     * 线程进入休眠
     */
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

    /**
     * 休眠唤醒,结束休眠
     */
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

    /**
     * 返回当前线程cpu使用率，范围为 0 ~ 100
     * @return 当前线程cpu使用率
     */
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

/**
 * 任务执行器
 */
class TaskExecutor : public ThreadLoadCounter{
public:
    typedef function<void()> Task;
    typedef shared_ptr<TaskExecutor> Ptr;

    /**
     * 构造函数
     * @param max_size cpu负载统计样本数
     * @param max_usec cpu负载统计时间窗口大小
     */
    TaskExecutor(uint64_t max_size = 32,uint64_t max_usec = 2 * 1000 * 1000):ThreadLoadCounter(max_size,max_usec){}
    virtual ~TaskExecutor(){}
    /**
     * 异步执行任务
     * @param task 任务
     * @param may_sync 是否允许同步执行该任务
     * @return 任务是否添加成功
     */
    virtual bool async(const Task &task, bool may_sync = true) = 0;

    /**
     * 最高优先级方式异步执行任务
     * @param task 任务
     * @param may_sync 是否允许同步执行该任务
     * @return 任务是否添加成功
     */
    virtual bool async_first(const Task &task, bool may_sync = true) {
        return async(task,may_sync);
    };

    /**
     * 同步执行任务
     * @param task
     * @return
     */
    virtual bool sync(const Task &task) = 0;

    /**
     * 最高优先级方式同步执行任务
     * @param task
     * @return
     */
    virtual bool sync_first(const Task &task) {
        return sync(task);
    };

    /**
     * 等待执行线程退出
     */
    virtual void wait() = 0;

    /**
     * 通知执行线程退出
     */
    virtual void shutdown() = 0;
};

class TaskExecutorGetter {
public:
    typedef shared_ptr<TaskExecutorGetter> Ptr;
    virtual ~TaskExecutorGetter(){};
    /**
     * 获取任务执行器
     * @return 任务执行器
     */
    virtual TaskExecutor::Ptr getExecutor() = 0;

    /**
     * 等待所有任务执行线程退出
     */
    virtual void wait() = 0;

    /**
     * 通知所有执行线程退出
     */
    virtual void shutdown() = 0;
};


class TaskExecutorGetterImp : public TaskExecutorGetter{
public:
    /**
     *
     * @tparam FUN 任务执行器创建方式
     * @param fun 任务执行器创建lambad
     * @param threadnum 任务执行器个数，默认cpu核心数
     */
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

    /**
     * 根据线程负载情况，获取最空闲的任务执行器
     * @return 任务执行器
     */
    TaskExecutor::Ptr getExecutor() override{
        auto thread_pos = _thread_pos;
        if(thread_pos >= _threads.size()){
            thread_pos = 0;
        }

        TaskExecutor::Ptr executor_min_load = _threads[thread_pos];
        auto min_load = executor_min_load->load();

        for(int i = 0; i < _threads.size() ; ++i , ++thread_pos ){
            if(thread_pos >= _threads.size()){
                thread_pos = 0;
            }

            auto th = _threads[thread_pos];
            auto load = th->load();

            if(load < min_load){
                min_load = load;
                executor_min_load = th;
            }
            if(min_load == 0){
                break;
            }
        }
        _thread_pos = thread_pos;
        return executor_min_load;
    }

    /**
     * 等待所有线程退出
     */
    void wait() override{
        for (auto &th : _threads){
            th->wait();
        }
    }
    /**
     *  关闭所有线程
     */
    void shutdown() override{
        for (auto &th : _threads){
            th->shutdown();
        }
    }

    /**
     * 获取所有线程的负载率
     * @return 所有线程的负载率
     */
    vector<int> getExecutorLoad(){
        vector<int> vec(_threads.size());
        int i = 0;
        for(auto &executor : _threads){
            vec[i++] = executor->load();
        }
        return vec;
    }

    /**
     * 获取所有线程任务执行延时，单位毫秒
     * 通过此函数也可以大概知道线程负载情况
     * @return
     */
    void getExecutorDelay(const function<void(const vector<int> &)> &callback){
        int totalCount = _threads.size();
        std::shared_ptr<atomic_int> completed = std::make_shared<atomic_int>(0);
        std::shared_ptr<vector<int> > delayVec = std::make_shared<vector<int>>(totalCount);
        int index = 0;
        for (auto &th : _threads){
            std::shared_ptr<Ticker> ticker = std::make_shared<Ticker >();
            th->async([completed,totalCount,delayVec,index,ticker,callback](){
                (*delayVec)[index] = ticker->elapsedTime();
                if(++(*completed) == totalCount){
                    callback((*delayVec));
                }
            }, false);
            ++index;
        }
    }
protected:
    vector <TaskExecutor::Ptr > _threads;
    int _thread_pos = 0;
};

}//toolkit


#endif //ZLTOOLKIT_TASKEXECUTOR_H
