/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <csignal>
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/NoticeCenter.h"
using namespace std;
using namespace toolkit;

//广播名称1  [AUTO-TRANSLATED:a6c535ee]
// Broadcast Name 1
#define NOTICE_NAME1 "NOTICE_NAME1"
//广播名称2  [AUTO-TRANSLATED:50cd7f16]
// Broadcast Name 2
#define NOTICE_NAME2 "NOTICE_NAME2"

//程序退出标记  [AUTO-TRANSLATED:2fc12083]
// Program Exit Flag
bool g_bExitFlag = false;


int main() {
    //设置程序退出信号处理函数  [AUTO-TRANSLATED:419fb1c3]
    // Set Program Exit Signal Handler
    signal(SIGINT, [](int){g_bExitFlag = true;});
    //设置日志  [AUTO-TRANSLATED:50372045]
    // Set Log
    Logger::Instance().add(std::make_shared<ConsoleChannel>());

    //对事件NOTICE_NAME1新增一个监听  [AUTO-TRANSLATED:c8e83e55]
    // Add a Listener to the Event NOTICE_NAME1
    //addListener方法第一个参数是标签，用来删除监听时使用  [AUTO-TRANSLATED:918a506c]
    // The First Parameter of the addListener Method is a Tag, Used to Delete the Listener
    //需要注意的是监听回调的参数列表个数类型需要与emitEvent广播时的完全一致，否则会有无法预知的错误  [AUTO-TRANSLATED:b36668f5]
    // Note that the Number and Type of Parameters in the Listener Callback Must be Exactly the Same as Those in the emitEvent Broadcast, Otherwise Unpredictable Errors May Occur
    NoticeCenter::Instance().addListener(0,NOTICE_NAME1,
            [](int &a,const char * &b,double &c,string &d){
        DebugL << a << " " << b << " " << c << " " << d;
        NoticeCenter::Instance().delListener(0,NOTICE_NAME1);

        NoticeCenter::Instance().addListener(0,NOTICE_NAME1,
                                             [](int &a,const char * &b,double &c,string &d){
                                                 InfoL << a << " " << b << " " << c << " " << d;
                                             });
    });

    //监听NOTICE_NAME2事件  [AUTO-TRANSLATED:36bfaf8a]
    // Listen for the NOTICE_NAME2 Event
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
        //每隔1秒广播一次事件，如果无法确定参数类型，可加强制转换  [AUTO-TRANSLATED:dc815907]
        // Broadcast the Event Every 1 Second, If the Parameter Type is Uncertain, a Forced Conversion Can be Added
        NoticeCenter::Instance().emitEvent(NOTICE_NAME1,++a,(const char *)"b",c,d);
        NoticeCenter::Instance().emitEvent(NOTICE_NAME2,d,c,b,a);
        sleep(1); // sleep 1 second
    }
    return 0;
}
