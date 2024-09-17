/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include "Util/logger.h"
#include "Network/Socket.h"
using namespace std;
using namespace toolkit;

class TestLog
{
public:
    template<typename T>
    TestLog(const T &t){
        _ss << t;
    };
    ~TestLog(){};

    //通过此友元方法，可以打印自定义数据类型  [AUTO-TRANSLATED:f9d17d41]
    // Through this friend method, you can print custom data types
    friend ostream& operator<<(ostream& out,const TestLog& obj){
        return out << obj._ss.str();
    }
private:
    stringstream _ss;
};

int main() {
    //初始化日志系统  [AUTO-TRANSLATED:25c549de]
    // Initialize the logging system
    Logger::Instance().add(std::make_shared<ConsoleChannel> ());
    Logger::Instance().add(std::make_shared<FileChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    InfoL << "测试std::cout风格打印：";
    //ostream支持的数据类型都支持,可以通过友元的方式打印自定义类型数据  [AUTO-TRANSLATED:c857af94]
    // All data types supported by ostream are supported, and custom type data can be printed through friend methods
    TraceL << "object int"<< TestLog((int)1)  << endl;
    DebugL << "object short:"<<TestLog((short)2)  << endl;
    InfoL << "object float:" << TestLog((float)3.12345678)  << endl;
    WarnL << "object double:" << TestLog((double)4.12345678901234567)  << endl;
    ErrorL << "object void *:" << TestLog((void *)0x12345678) << endl;
    ErrorL << "object string:" << TestLog("test string") << endl;

    //这是ostream原生支持的数据类型  [AUTO-TRANSLATED:c431abc8]
    // These are the data types natively supported by ostream
    TraceL << "int"<< (int)1  << endl;
    DebugL << "short:"<< (short)2  << endl;
    InfoL << "float:" << (float)3.12345678  << endl;
    WarnL << "double:" << (double)4.12345678901234567  << endl;
    ErrorL << "void *:" << (void *)0x12345678 << endl;
    //根据RAII的原理，此处不需要输入 endl，也会在被函数栈pop时打印log  [AUTO-TRANSLATED:1ca0e0f7]
    // Based on the RAII principle, there is no need to input endl here, and the log will be printed when the function is popped from the stack
    ErrorL << "without endl!";

    PrintI("测试printf风格打印：");
    PrintT("this is a %s test:%d", "printf trace", 124);
    PrintD("this is a %s test:%p", "printf debug", (void*)124);
    PrintI("this is a %s test:%c", "printf info", 'a');
    PrintW("this is a %s test:%X", "printf warn", 0x7F);
    PrintE("this is a %s test:%x", "printf err", 0xab);

    LogI("测试可变长度模板样式打印：");
    LogT(1, "+", "2", '=', 3);
    LogD(1, "+", "2", '=', 3);
    LogI(1, "+", "2", '=', 3);
    LogW(1, "+", "2", '=', 3);
    LogE(1, "+", "2", '=', 3);


    for (int i = 0; i < 2; ++i) {
        DebugL << "this is a repeat 2 times log";
        this_thread::sleep_for(chrono::milliseconds(10));
    }

    for (int i = 0; i < 3; ++i) {
        DebugL << "this is a repeat 3 times log";
        this_thread::sleep_for(chrono::milliseconds(10));
    }

    for (int i = 0; i < 100; ++i) {
        DebugL << "this is a repeat 100 log";
        this_thread::sleep_for(chrono::milliseconds(10));
    }

    toolkit::SockException ex((ErrCode)1, "test");
    DebugL << "sock exception: " << ex;

    InfoL << "done!";
    return 0;
}
