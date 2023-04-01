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
#include "Util/mini.h"
using namespace std;
using namespace toolkit;

int main() {
    //初始化日志系统
    Logger::Instance().add(std::make_shared<ConsoleChannel> ());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
    mINI ini;
    ini[".dot"] = "dot-value";
    ini["no-dot"] = "no-dot-value";
    ini["no-key-filed."] = "no-key-value";
    ini["field0.multi.dot"] = "filed.multi.dot-value";

    ini["field0.str"] = "value";
    ini["field0.int"] = 1;
    ini["field0.bool"] = true;

    ini["field1.str"] = "value";
    ini["field1.int"] = 1;
    ini["field1.bool"] = true;

    auto str = ini.dump();
    InfoL << "\n" << str;

    ini.clear();
    ini.parse(str);
    for (auto &pr: ini) {
        DebugL << pr.first << " = " << pr.second;
    }

    auto ini_str = R"(
        no—field=value

        [filed]
        a-key
        b-key=
        c-key=test
        ; comment0
        d-key = test
        # comment1
        e-key =
        =no-key
        multi.dot=multi.dot.value
    )";
    ini.clear();
    ini.parse(ini_str);
    for (auto &pr: ini) {
        TraceL << pr.first << " = " << pr.second;
    }

    return 0;
}
