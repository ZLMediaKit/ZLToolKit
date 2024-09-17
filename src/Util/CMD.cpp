/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "CMD.h"
#include "onceToken.h"

#if defined(_WIN32)
#include "win32/getopt.h"
#else
#include <getopt.h>
#endif // defined(_WIN32)

using namespace std;

namespace toolkit {

//默认注册exit/quit/help/clear命令  [AUTO-TRANSLATED:1411f05e]
//Default registration of exit/quit/help/clear commands
static onceToken s_token([]() {
    REGIST_CMD(exit)
    REGIST_CMD(quit)
    REGIST_CMD(help)
    REGIST_CMD(clear)
});

CMDRegister &CMDRegister::Instance() {
    static CMDRegister instance;
    return instance;
}

void OptionParser::operator()(mINI &all_args, int argc, char *argv[], const std::shared_ptr<ostream> &stream) {
    vector<struct option> vec_long_opt;
    string str_short_opt;
    do {
        struct option tmp;
        for (auto &pr : _map_options) {
            auto &opt = pr.second;
            //long opt
            tmp.name = (char *) opt._long_opt.data();
            tmp.has_arg = opt._type;
            tmp.flag = nullptr;
            tmp.val = pr.first;
            vec_long_opt.emplace_back(tmp);
            //short opt
            if (!opt._short_opt) {
                continue;
            }
            str_short_opt.push_back(opt._short_opt);
            switch (opt._type) {
                case Option::ArgRequired: str_short_opt.append(":"); break;
                case Option::ArgOptional: str_short_opt.append("::"); break;
                default: break;
            }
        }
        tmp.flag = 0;
        tmp.name = 0;
        tmp.has_arg = 0;
        tmp.val = 0;
        vec_long_opt.emplace_back(tmp);
    } while (0);

    static mutex s_mtx_opt;
    lock_guard<mutex> lck(s_mtx_opt);

    int index;
    optind = 0;
    opterr = 0;
    while ((index = getopt_long(argc, argv, &str_short_opt[0], &vec_long_opt[0], nullptr)) != -1) {
        stringstream ss;
        ss << "  未识别的选项,输入\"-h\"获取帮助.";
        if (index < 0xFF) {
            //短参数  [AUTO-TRANSLATED:87b4c1df]
            //Short parameters
            auto it = _map_char_index.find(index);
            if (it == _map_char_index.end()) {
                throw std::invalid_argument(ss.str());
            }
            index = it->second;
        }

        auto it = _map_options.find(index);
        if (it == _map_options.end()) {
            throw std::invalid_argument(ss.str());
        }
        auto &opt = it->second;
        auto pr = all_args.emplace(opt._long_opt, optarg ? optarg : "");
        if (!opt(stream, pr.first->second)) {
            return;
        }
        optarg = nullptr;
    }
    for (auto &pr : _map_options) {
        if (pr.second._default_value && all_args.find(pr.second._long_opt) == all_args.end()) {
            //有默认值,赋值默认值  [AUTO-TRANSLATED:9a82f49c]
            //Has default value, assigns default value
            all_args.emplace(pr.second._long_opt, *pr.second._default_value);
        }
    }
    for (auto &pr : _map_options) {
        if (pr.second._must_exist) {
            if (all_args.find(pr.second._long_opt) == all_args.end()) {
                stringstream ss;
                ss << "  参数\"" << pr.second._long_opt << "\"必须提供,输入\"-h\"选项获取帮助";
                throw std::invalid_argument(ss.str());
            }
        }
    }
    if (all_args.empty() && _map_options.size() > 1 && !_enable_empty_args) {
        _helper(stream, "");
        return;
    }
    if (_on_completed) {
        _on_completed(stream, all_args);
    }
}

}//namespace toolkit