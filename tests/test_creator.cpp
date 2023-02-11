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
using namespace std;
using namespace toolkit;

//测试onCreate和onDestory同时存在
class TestA {
public:
    TestA() {
        TraceL;
    }

    ~TestA() {
        TraceL;
    }

    void onCreate() {
        TraceL << demangle(typeid(*this).name()) << "::" << __FUNCTION__;
    }

    void onDestory() {
        TraceL << demangle(typeid(*this).name()) << "::" << __FUNCTION__;
    }
};

//测试只存在onCreate
class TestB {
public:
    TestB() {
        TraceL;
    }

    ~TestB() {
        TraceL;
    }

    void onCreate() {
        TraceL << demangle(typeid(*this).name()) << "::" << __FUNCTION__;
    }
};

//测试只存在onDestory
class TestC {
public:
    TestC() {
        TraceL;
    }

    ~TestC() {
        TraceL;
    }

    void onDestory() {
        TraceL << demangle(typeid(*this).name()) << "::" << __FUNCTION__;
    }
};

//测试onCreate和onDestory返回值不为void时
class TestD {
public:
    TestD() {
        TraceL;
    }

    ~TestD() {
        TraceL;
    }

    int onCreate() {
        TraceL << demangle(typeid(*this).name()) << "::" << __FUNCTION__;
        return 1;
    }

    std::string onDestory() {
        TraceL << demangle(typeid(*this).name()) << "::" << __FUNCTION__;
        return "test";
    }
};

//测试onCreate和onDestory都不存在时
class TestE {
public:
    TestE() {
        TraceL;
    }

    ~TestE() {
        TraceL;
    }
};

//测试自定义构造函数
class TestF {
public:
    TestF(int a, const char *b) {
        TraceL << a << " " << b;
    }

    ~TestF() {
        TraceL;
    }
};

//测试自定义onCreate函数
class TestH {
public:
    TestH() {
        TraceL;
    }

    int onCreate(int a = 0, const char *b = nullptr) {
        TraceL << demangle(typeid(*this).name()) << "::" << __FUNCTION__ << " " << a << " " << b;
        return 10;
    }

    ~TestH() {
        TraceL;
    }
};

//测试onDestory函数抛异常
class TestI {
public:
    TestI() {
        TraceL;
    }

    int onDestory() {
        TraceL << demangle(typeid(*this).name()) << "::" << __FUNCTION__;
        throw std::runtime_error("TestI");
    }

    ~TestI() {
        TraceL;
    }
};

//测试自定义onDestory，onDestory将被忽略调用
class TestJ {
public:
    TestJ() {
        TraceL;
    }

    int onDestory(int a) {
        return a;
    }

    ~TestJ() {
        TraceL;
    }
};

int main() {
    //初始化日志系统
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    Creator::create<TestA>();
    Creator::create<TestB>();
    Creator::create<TestC>();
    Creator::create<TestD>();
    Creator::create<TestE>();
    Creator::create<TestF>(1, "hellow");
    {
        auto h = Creator::create2<TestH>(1, "hellow");
        TraceL << "invoke TestH onCreate ret:" << CLASS_FUNC_INVOKE(TestH, *h, Create, 1, "hellow");
    }

    Creator::create<TestI>();
    Creator::create<TestJ>();
    return 0;
}
