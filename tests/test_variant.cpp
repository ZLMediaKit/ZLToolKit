/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Util/logger.h"
#include "Util/mini.h"

using namespace toolkit;

int main() {
    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    mINI ini;
    ini["a"] = "true";
    ini["b"] = "false";
    ini["c"] = "123";

    InfoL << ini["a"].as<bool>() << (bool) ini["a"];
    InfoL << ini["b"].as<bool>() << (bool) ini["b"];
    InfoL << ini["c"].as<int>() << (int) ini["c"];
    InfoL << (int)(ini["c"].as<uint8_t>()) << (int)((uint8_t) ini["c"]);

    return 0;
}
