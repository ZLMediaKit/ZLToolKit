/*
* MIT License
*
* Copyright (c) 2016 xiongziliang <771730766@qq.com>
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#include <iostream>
#if !defined(_WIN32)
#include <dirent.h>
#endif //!defined(_WIN32)

#include <unordered_set>
#include "Util/logger.h"
#include "Util/uv_errno.h"
#include "Util/File.h"
#include "Util/TimeTicker.h"
#include "Util/MD5.h"
#include "Util/util.h"
#include "Util/onceToken.h"
#include "Util/mini.h"

using namespace std;
using namespace toolkit;

#define MASK_FUNC remove_file_header
//限制时间30天
#define TIME_LIMIT (1547697774 + (30 * 24 * 60 * 60))

const char flv_file_header[] = "FLV\x1\x5\x0\x0\x0\x9\x0\x0\x0\x0"; // have audio and have video

namespace Enc {
#define ENC_FIELD "encryption."
    const char kKey[] = ENC_FIELD"key";
    const char kInDir[] = ENC_FIELD"in_dir";
    const char kOutDir[] = ENC_FIELD"out_dir";
    onceToken token([]() {
        mINI::Instance()[kKey] = makeRandStr(8);
        mINI::Instance()[kInDir] = "C:\\in_dir\\";
        mINI::Instance()[kOutDir] = "C:\\out_dir\\";
    }, nullptr);
}//namespace Enc


int mask_file(const string &mask, const string &in_file, const string &out_file_tmp) {
    string out_file = out_file_tmp;
    FILE *fp_in = fopen(in_file.data(), "rb");
    if (!fp_in) {
        ErrorL << "打开输入文件失败:" << in_file << " " << get_uv_errmsg();
        return -1;
    }
    if (out_file.empty()) {
        auto vec = split(in_file, ".");
        if (vec.size() == 2) {
            out_file = vec[0] + "_mask." + vec[1];
        }
        else {
            out_file = vec[0] + "_mask";
        }
    }

    FILE *fp_out = File::createfile_file(out_file.data(), "wb");
    if (!fp_out) {
        ErrorL << "打开输出文件失败:" << in_file << " " << get_uv_errmsg();;
        fclose(fp_in);
        return -1;
    }

    unsigned char *mask_str = (unsigned char *)mask.data();
    int mask_len = mask.size();
    auto file_offset = 0LL;

    unsigned char buf[4 * 1024];

    DebugL << "开始加密或解密" << in_file << ",掩码为:" << mask_str;
    Ticker ticker;
    while (true) {
        int ret = fread(buf, 1, sizeof(buf), fp_in);

        if (ret > 0) {
            unsigned char *ptr = buf;
            for (int i = 0; i < ret; ++i, ++ptr) {
                *(ptr) ^= mask_str[(file_offset + i) % mask_len];
            }
            file_offset += ret;
            fwrite(buf, ret, 1, fp_out);
        }

        if (ret != sizeof(buf)) {
            break;
        }
    }
    fclose(fp_in);
    fclose(fp_out);

    DebugL << "完成文件加密或解密:" << out_file << ",耗时" << ticker.elapsedTime() << "毫秒";
    return 0;
}

int add_file_header(const string &mask_tmp, const string &in_file, const string &out_file_tmp) {
    auto mask = MD5(mask_tmp).hexdigest();
    string out_file = out_file_tmp;
    FILE *fp_in = fopen(in_file.data(), "rb");
    if (!fp_in) {
        ErrorL << "打开输入文件失败:" << in_file << " " << get_uv_errmsg();
        return -1;
    }
    if (out_file.empty()) {
        auto vec = split(in_file, ".");
        if (vec.size() == 2) {
            out_file = vec[0] + "_加密." + vec[1];
        }
        else {
            out_file = vec[0] + "_加密";
        }
    }

    FILE *fp_out = File::createfile_file(out_file.data(), "wb");
    if (!fp_out) {
        ErrorL << "打开输出文件失败:" << in_file << " " << get_uv_errmsg();;
        fclose(fp_in);
        return -1;
    }

    {
        //写FLV文件头
        fwrite(flv_file_header, sizeof(flv_file_header) - 1, 1, fp_out);
        int16_t mask_len = htons(mask.size());
        fwrite(&mask_len, sizeof(mask_len), 1, fp_out);
        fwrite(mask.data(), mask.size(), 1, fp_out);
    }

    unsigned char buf[64 * 1024];
    DebugL << "开始加密" << in_file << ",密码为:" << mask;
    Ticker ticker;
    while (true) {
        int ret = fread(buf, 1, sizeof(buf), fp_in);
        if (ret > 0) {
            fwrite(buf, ret, 1, fp_out);
        }
        if (ret != sizeof(buf)) {
            break;
        }
    }
    fclose(fp_in);
    fclose(fp_out);

    DebugL << "完成文件加密:" << out_file << ",耗时" << ticker.elapsedTime() << "毫秒";
    return 0;
}


int remove_file_header(const string &, const string &in_file, const string &out_file_tmp) {
    string out_file = out_file_tmp;
    FILE *fp_in = fopen(in_file.data(), "rb");
    if (!fp_in) {
        ErrorL << "打开输入文件失败:" << in_file << " " << get_uv_errmsg();
        return -1;
    }
    if (out_file.empty()) {
        auto vec = split(in_file, ".");
        if (vec.size() == 2) {
            out_file = vec[0] + "_解密." + vec[1];
        }
        else {
            out_file = vec[0] + "_解密";
        }
    }

    FILE *fp_out = File::createfile_file(out_file.data(), "wb");
    if (!fp_out) {
        ErrorL << "打开输出文件失败:" << in_file << " " << get_uv_errmsg();;
        fclose(fp_in);
        return -1;
    }

    unsigned char buf[64 * 1024];
    DebugL << "开始解密" << in_file;

    {
        int totalHeaderLen = sizeof(flv_file_header) - 1 + sizeof(int16_t);
        auto ret = fread(buf, 1, totalHeaderLen, fp_in);
        if (ret < totalHeaderLen) {
            ErrorL << "文件长度不够:" << in_file << " " << ret << " < " << totalHeaderLen;
            return -1;
        }

        if (memcmp(flv_file_header, buf, sizeof(flv_file_header) - 1) != 0) {
            ErrorL << "该文件不是已加密的文件:" << in_file << " " << hexdump(buf, sizeof(flv_file_header) - 1);
            return -1;
        }
        int16_t mask_len;
        memcpy(&mask_len, buf + sizeof(flv_file_header) - 1, sizeof(int16_t));
        mask_len = ntohs(mask_len);
        ret = fread(buf, 1, mask_len, fp_in);
        if (ret != mask_len) {
            ErrorL << "文件长度不够:" << in_file << " " << ret << " < " << mask_len;
            return -1;
        }
        InfoL << "文件密码为:" << string((char *)buf, mask_len);
    }
    Ticker ticker;
    while (true) {
        int ret = fread(buf, 1, sizeof(buf), fp_in);
        if (ret > 0) {
            fwrite(buf, ret, 1, fp_out);
        }
        if (ret != sizeof(buf)) {
            break;
        }
    }
    fclose(fp_in);
    fclose(fp_out);

    DebugL << "完成文件加密:" << out_file << ",耗时" << ticker.elapsedTime() << "毫秒";
    return 0;
}


template <typename FUN>
void for_each_file_in_dir(const string &strPathPrefix, FUN &&fun) {
    DIR *pDir;
    dirent *pDirent;
    if ((pDir = opendir(strPathPrefix.data())) == NULL) {
        return;
    }
    unordered_set<string> setFile;
    while ((pDirent = readdir(pDir)) != NULL) {
        if (File::is_special_dir(pDirent->d_name)) {
            continue;
        }
        if (pDirent->d_name[0] == '.') {
            continue;
        }
        setFile.emplace(pDirent->d_name);
    }
    for (auto &strFile : setFile) {
        string strAbsolutePath = strPathPrefix + "/" + strFile;
        if (File::is_dir(strAbsolutePath.data())) {
            for_each_file_in_dir(strAbsolutePath, std::forward<FUN>(fun));
        }
        else { //是文件
            fun(strAbsolutePath.data());
        }
    }
    closedir(pDir);
}



bool loadIniConfig(const char *ini_path) {
    string ini;
    if (ini_path) {
        ini = ini_path;
    }
    else {
        ini = exePath() + ".ini";
    }
    try {
        mINI::Instance().parseFile(ini);
        return true;
    }
    catch (std::exception &ex) {
        ofstream out(ini, ios::out | ios::binary | ios::trunc);
        auto dmp = mINI::Instance().dump("", "");
        out.write(dmp.data(), dmp.size());
        return false;
    }
}

int main(int argc, char *argv[]) {
    //初始化日志系统
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    if (TIME_LIMIT  > 0 && time(NULL) > TIME_LIMIT) {
        ErrorL << "软件授权已过期";
        return -1;
    }

    //加载配置文件，如果配置文件不存在就创建一个
    loadIniConfig((exeDir() + "config.ini").data());

    auto mask = mINI::Instance()[Enc::kKey];
    auto in_file = mINI::Instance()[Enc::kInDir];
    auto out_file = mINI::Instance()[Enc::kOutDir];

    if (!File::is_dir(in_file.data())) {
        MASK_FUNC(mask, in_file, out_file);
    }
    else {
        InfoL << "开始遍历文件夹:" << in_file;
        if (out_file.back() != '/' && out_file.back() != '\\') {
#ifdef _WIN32
            out_file.append("\\");
#else
            out_file.append("/");
#endif
        }

        for_each_file_in_dir(in_file, [&](const string &file) {
#ifdef _WIN32
            auto vec = split(file, "\\");
#else
            auto vec = split(file, "/");
#endif
            auto file_name = vec.back();
            MASK_FUNC(mask, file, out_file + file_name);
        });
    }
    InfoL << "输入回车符退出本程序:" << endl;
    getchar();
    return 0;
}
