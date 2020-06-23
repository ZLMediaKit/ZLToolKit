/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */
 
#ifndef UTIL_WORKTHREADPOOL_H_
#define UTIL_WORKTHREADPOOL_H_

#include <memory>
#include "ThreadPool.h"
#include "Poller/EventPoller.h"
using namespace std;

namespace toolkit {

class WorkThreadPool :
        public std::enable_shared_from_this<WorkThreadPool> ,
        public TaskExecutorGetterImp {
public:
    typedef std::shared_ptr<WorkThreadPool> Ptr;
    ~WorkThreadPool(){};

    /**
     * 获取单例
     * @return
     */
    static WorkThreadPool &Instance();

    /**
     * 设置EventPoller个数，在WorkThreadPool单例创建前有效
     * 在不调用此方法的情况下，默认创建thread::hardware_concurrency()个EventPoller实例
     * @param size  EventPoller个数，如果为0则为thread::hardware_concurrency()
     */
    static void setPoolSize(int size = 0);

    /**
     * 获取第一个实例
     * @return
     */
    EventPoller::Ptr getFirstPoller();

    /**
     * 根据负载情况获取轻负载的实例
     * 如果优先返回当前线程，那么会返回当前线程
     * 返回当前线程的目的是为了提高线程安全性
     * @return
     */
    EventPoller::Ptr getPoller();
private:
    WorkThreadPool() ;
private:
    static int s_pool_size;
};

} /* namespace toolkit */
#endif /* UTIL_WORKTHREADPOOL_H_ */
