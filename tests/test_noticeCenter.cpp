/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <signal.h>
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/NoticeCenter.h"
using namespace std;
using namespace toolkit;

//广播名称1
#define NOTICE_NAME1 "NOTICE_NAME1"
//广播名称2
#define NOTICE_NAME2 "NOTICE_NAME2"

//程序退出标记
bool g_bExitFlag = false;


int main() {
    //设置程序退出信号处理函数
    signal(SIGINT, [](int){g_bExitFlag = true;});
    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>());

    //对事件NOTICE_NAME1新增一个监听
    //addListener方法第一个参数是标签，用来删除监听时使用
    //需要注意的是监听回调的参数列表个数类型需要与emitEvent广播时的完全一致，否则会有无法预知的错误
    NoticeCenter::Instance().addListener(0,NOTICE_NAME1,
            [](int &a,const char * &b,double &c,string &d){
        DebugL << a << " " << b << " " << c << " " << d;
        NoticeCenter::Instance().delListener(0,NOTICE_NAME1);

        NoticeCenter::Instance().addListener(0,NOTICE_NAME1,
                                             [](int &a,const char * &b,double &c,string &d){
                                                 InfoL << a << " " << b << " " << c << " " << d;
                                             });
    });

    //监听NOTICE_NAME2事件
    NoticeCenter::Instance().addListener(0,NOTICE_NAME2,
            [](string &d,double &c,const char *&b,int &a){
        DebugL << a << " " << b << " " << c << " " << d;
        NoticeCenter::Instance().delListener(0,NOTICE_NAME2);

        NoticeCenter::Instance().addListener(0,NOTICE_NAME2,
                                             [](string &d,double &c,const char *&b,int &a){
                                                 WarnL << a << " " << b << " " << c << " " << d;
                                             });

    });
    int a = 0;
    while(!g_bExitFlag){
        const char *b = "b";
        double c = 3.14;
        string d("d");
        //每隔1秒广播一次事件，如果无法确定参数类型，可加强制转换
        NoticeCenter::Instance().emitEvent(NOTICE_NAME1,++a,(const char *)"b",c,d);
        NoticeCenter::Instance().emitEvent(NOTICE_NAME2,d,c,b,a);
        sleep(1); // sleep 1 second
    }
    return 0;
}
