/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xia-chu/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include <mutex>
#include <string>
#include <algorithm>
#include <unordered_map>

#include "util.h"
#include "onceToken.h"
#include "Util/File.h"
#include "Util/logger.h"
#include "Util/uv_errno.h"
#include "Network/sockutil.h"

#if defined(_WIN32)
#include <shlwapi.h>  
#pragma comment(lib, "shlwapi.lib")
#endif // defined(_WIN32)

#if defined(__MACH__) || defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach-o/dyld.h> /* _NSGetExecutablePath */
int uv_exepath(char* buffer, int *size) {
    /* realpath(exepath) may be > PATH_MAX so double it to be on the safe side. */
    char abspath[PATH_MAX * 2 + 1];
    char exepath[PATH_MAX + 1];
    uint32_t exepath_size;
    size_t abspath_size;

    if (buffer == NULL || size == NULL || *size == 0)
        return -EINVAL;

    exepath_size = sizeof(exepath);
    if (_NSGetExecutablePath(exepath, &exepath_size))
        return -EIO;

    if (realpath(exepath, abspath) != abspath)
        return -errno;

    abspath_size = strlen(abspath);
    if (abspath_size == 0)
        return -EIO;

    *size -= 1;
    if (*size > abspath_size)
        *size = abspath_size;

    memcpy(buffer, abspath, *size);
    buffer[*size] = '\0';

    return 0;
}
#endif //defined(__MACH__) || defined(__APPLE__)

using namespace std;

namespace toolkit {

string makeRandStr(int sz, bool printable) {
    char *tmp = new char[sz + 1];
    static const char CCH[] =
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int i;
    for (i = 0; i < sz; i++) {
        srand((unsigned)time(NULL) + i);
        if (printable) {
            int x = rand() % (sizeof(CCH) - 1);
            tmp[i] = CCH[x];
        }
        else {
            tmp[i] = rand() % 0xFF;
        }
    }
    tmp[i] = 0;
    string ret = tmp;
    delete[] tmp;
    return ret;
}

bool is_safe(uint8_t b) {
    return b >= ' ' && b < 128;
}
string hexdump(const void *buf, size_t len) {
    string ret("\r\n");
    char tmp[8];
    const uint8_t *data = (const uint8_t *)buf;
    for (size_t i = 0; i < len; i += 16) {
        for (int j = 0; j < 16; ++j) {
            if (i + j < len) {
                int sz = snprintf(tmp, sizeof(tmp), "%.2x ", data[i + j]);
                ret.append(tmp, sz);
            }
            else {
                int sz = snprintf(tmp, sizeof(tmp), "   ");
                ret.append(tmp, sz);
            }
        }
        for (int j = 0; j < 16; ++j) {
            if (i + j < len) {
                ret += (is_safe(data[i + j]) ? data[i + j] : '.');
            }
            else {
                ret += (' ');
            }
        }
        ret += ('\n');
    }
    return ret;
}

string hexmem(const void* buf, size_t len) {
	string ret;
	char tmp[8];
	const uint8_t* data = (const uint8_t*)buf;
	for (size_t i = 0; i < len; ++i) {
		int sz = sprintf(tmp, "%.2x ", data[i]);
		ret.append(tmp, sz);
	}
	return ret;
}

string exePath() {
    char buffer[PATH_MAX * 2 + 1] = { 0 };
    int n = -1;
#if defined(_WIN32)
    n = GetModuleFileNameA(NULL, buffer, sizeof(buffer));
#elif defined(__MACH__) || defined(__APPLE__)
    n = sizeof(buffer);
    if (uv_exepath(buffer, &n) != 0) {
        n = -1;
    }
#elif defined(__linux__)
    n = readlink("/proc/self/exe", buffer, sizeof(buffer));
#endif

    string filePath;
    if (n <= 0) {
        filePath = "./";
    }
    else {
        filePath = buffer;
    }

#if defined(_WIN32)
    //windows下把路径统一转换层unix风格，因为后续都是按照unix风格处理的
    for (auto &ch : filePath) {
        if (ch == '\\') {
            ch = '/';
        }
    }
#endif //defined(_WIN32)

    return filePath;
}

string exeDir() {
    auto path = exePath();
    return path.substr(0, path.rfind('/') + 1);
}

string exeName() {
    auto path = exePath();
    return path.substr(path.rfind('/') + 1);
}

// string转小写
std::string &strToLower(std::string &str) {
    transform(str.begin(), str.end(), str.begin(), towlower);
    return str;
}

// string转大写
std::string &strToUpper(std::string &str) {
    transform(str.begin(), str.end(), str.begin(), towupper);
    return str;
}

// string转小写
std::string strToLower(std::string &&str) {
    transform(str.begin(), str.end(), str.begin(), towlower);
    return std::move(str);
}

// string转大写
std::string strToUpper(std::string &&str) {
    transform(str.begin(), str.end(), str.begin(), towupper);
    return std::move(str);
}

vector<string> split(const string& s, const char *delim) {
    vector<string> ret;
    size_t last = 0;
    auto index = s.find(delim, last);
    while (index != string::npos) {
        if (index - last > 0) {
            ret.push_back(s.substr(last, index - last));
        }
        last = index + strlen(delim);
        index = s.find(delim, last);
    }
    if (!s.size() || s.size() - last > 0) {
        ret.push_back(s.substr(last));
    }
    return ret;
}

#define TRIM(s,chars) \
do{ \
    string map(0xFF, '\0'); \
    for (auto &ch : chars) { \
        map[(unsigned char &)ch] = '\1'; \
    } \
    while( s.size() && map.at((unsigned char &)s.back())) s.pop_back(); \
    while( s.size() && map.at((unsigned char &)s.front())) s.erase(0,1); \
}while(0);

//去除前后的空格、回车符、制表符
std::string& trim(std::string &s, const string &chars) {
    TRIM(s, chars);
    return s;
}
std::string trim(std::string &&s, const string &chars) {
    TRIM(s, chars);
    return std::move(s);
}

void replace(string &str, const string &old_str, const string &new_str) {
    if (old_str.empty() || old_str == new_str) {
        return;
    }
    auto pos = str.find(old_str);
    if (pos == string::npos) {
        return;
    }
    str.replace(pos, old_str.size(), new_str);
    replace(str, old_str, new_str);
}

bool start_with(const string &str, const string &substr) {
    return str.find(substr) == 0;
}

bool end_with(const string &str, const string &substr) {
    auto pos = str.rfind(substr);
    return pos != string::npos && pos == str.size() - substr.size();
}

bool isIP(const char *str){
    return INADDR_NONE != inet_addr(str);
}

#if defined(_WIN32)
void sleep(int second) {
    Sleep(1000 * second);
}
void usleep(int micro_seconds) {
    this_thread::sleep_for(std::chrono::microseconds(micro_seconds));
}

int gettimeofday(struct timeval *tp, void *tzp) {
    auto now_stamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    tp->tv_sec = (decltype(tp->tv_sec))(now_stamp / 1000000LL);
    tp->tv_usec = now_stamp % 1000000LL;
    return 0;
}

const char *strcasestr(const char *big, const char *little){
    string big_str = big;
    string little_str = little;
    strToLower(big_str);
    strToLower(little_str);
    auto pos = strstr(big_str.data(), little_str.data());
    if (!pos){
        return nullptr;
    }
    return big + (pos - big_str.data());
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int vasprintf(char **strp, const char *fmt, va_list ap) {
    // _vscprintf tells you how big the buffer needs to be
    int len = _vscprintf(fmt, ap);
    if (len == -1) {
        return -1;
    }
    size_t size = (size_t)len + 1;
    char *str = (char*)malloc(size);
    if (!str) {
        return -1;
    }
    // _vsprintf_s is the "secure" version of vsprintf
    int r = vsprintf_s(str, len + 1, fmt, ap);
    if (r == -1) {
        free(str);
        return -1;
    }
    *strp = str;
    return r;
}

 int asprintf(char **strp, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vasprintf(strp, fmt, ap);
    va_end(ap);
    return r;
}

#endif //WIN32


static inline uint64_t getCurrentMicrosecondOrigin() {
#if !defined(_WIN32)
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
#else
    return  std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
#endif
}
static atomic<uint64_t> s_currentMicrosecond(0);
static atomic<uint64_t> s_currentMillisecond(0);
static atomic<uint64_t> s_currentMicrosecond_system(getCurrentMicrosecondOrigin());
static atomic<uint64_t> s_currentMillisecond_system(getCurrentMicrosecondOrigin() / 1000);

static inline bool initMillisecondThread() {
    static std::thread s_thread([]() {
        setThreadName("stamp thread");
        DebugL << "Stamp thread started!";
        uint64_t last = getCurrentMicrosecondOrigin();
        uint64_t now;
        uint64_t microsecond = 0;
        while (true) {
            now = getCurrentMicrosecondOrigin();
            //记录系统时间戳，可回退
            s_currentMicrosecond_system.store(now, memory_order_release);
            s_currentMillisecond_system.store(now / 1000, memory_order_release);

            //记录流逝时间戳，不可回退
            int64_t expired = now - last;
            last = now;
            if (expired > 0 && expired < 1000 * 1000) {
                //流逝时间处于0~1000ms之间，那么是合理的，说明没有调整系统时间
                microsecond += expired;
                s_currentMicrosecond.store(microsecond, memory_order_release);
                s_currentMillisecond.store(microsecond / 1000, memory_order_release);
            } else if(expired != 0){
                WarnL << "Stamp expired is not abnormal:" << expired;
            }
            //休眠0.5 ms
            usleep(500);
        }
    });
    static onceToken s_token([]() {
        s_thread.detach();
    });
    return true;
}

uint64_t getCurrentMillisecond(bool system_time) {
    static bool flag = initMillisecondThread();
    if(system_time){
        return s_currentMillisecond_system.load(memory_order_acquire);
    }
    return s_currentMillisecond.load(memory_order_acquire);
}

uint64_t getCurrentMicrosecond(bool system_time) {
    static bool flag = initMillisecondThread();
    if(system_time){
        return s_currentMicrosecond_system.load(memory_order_acquire);
    }
    return s_currentMicrosecond.load(memory_order_acquire);
}

string getTimeStr(const char *fmt, time_t time) {
    if (!time) {
        time = ::time(NULL);
    }
    auto tm = getLocalTime(time);
    char buffer[64];
    auto success = std::strftime(buffer, sizeof(buffer), fmt, &tm);
    return 0 == success ? string(fmt) : buffer;
}

struct tm getLocalTime(time_t sec) {
    struct tm tm;
#ifdef _WIN32
    localtime_s(&tm, &sec);
#else
    localtime_r(&sec, &tm);
#endif //_WIN32
    return tm;
}

void setThreadName(const char *name) {
#if defined(__linux) || defined(__linux__)
    pthread_setname_np(pthread_self(), name);
#elif defined(__MACH__) || defined(__APPLE__)
    pthread_setname_np(name);
#endif
}

string getThreadName() {
#if defined(__linux) || defined(__linux__) || defined(__MACH__) || defined(__APPLE__)
    string ret;
    ret.resize(32);
    auto tid = pthread_self();
    pthread_getname_np(tid, (char *) ret.data(), ret.size());
    if (ret[0]) {
        ret.resize(strlen(ret.data()));
        return ret;
    }
    return to_string((uint64_t) tid);
#else
    std::ostringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
#endif
}

}  // namespace toolkit

