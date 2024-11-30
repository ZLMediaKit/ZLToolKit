/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include <random>

#include "util.h"
#include "local_time.h"
#include "File.h"
#include "onceToken.h"
#include "logger.h"
#include "uv_errno.h"
#include "Network/sockutil.h"

#if defined(_WIN32)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
extern "C" const IMAGE_DOS_HEADER __ImageBase;
#endif // defined(_WIN32)

#if defined(__MACH__) || defined(__APPLE__)
#include <limits.h>
#include <mach-o/dyld.h> /* _NSGetExecutablePath */

int uv_exepath(char *buffer, int *size) {
    /* realpath(exepath) may be > PATH_MAX so double it to be on the safe side. */
    char abspath[PATH_MAX * 2 + 1];
    char exepath[PATH_MAX + 1];
    uint32_t exepath_size;
    size_t abspath_size;

    if (buffer == nullptr || size == nullptr || *size == 0)
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
    if ((size_t) *size > abspath_size)
        *size = abspath_size;

    memcpy(buffer, abspath, *size);
    buffer[*size] = '\0';

    return 0;
}

#endif //defined(__MACH__) || defined(__APPLE__)

using namespace std;

#ifndef HAS_CXA_DEMANGLE
// We only support some compilers that support __cxa_demangle.
// TODO: Checks if Android NDK has fixed this issue or not.
#if defined(__ANDROID__) && (defined(__i386__) || defined(__x86_64__))
#define HAS_CXA_DEMANGLE 0
#elif (__GNUC__ >= 4 || (__GNUC__ >= 3 && __GNUC_MINOR__ >= 4)) && \
!defined(__mips__)
#define HAS_CXA_DEMANGLE 1
#elif defined(__clang__) && !defined(_MSC_VER)
#define HAS_CXA_DEMANGLE 1
#else
#define HAS_CXA_DEMANGLE 0
#endif
#endif
#if HAS_CXA_DEMANGLE
#include <cxxabi.h>
#endif

namespace toolkit {

static constexpr char CCH[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

string makeRandStr(int sz, bool printable) {
    string ret;
    ret.resize(sz);
    std::mt19937 rng(std::random_device{}());
    for (int i = 0; i < sz; ++i) {
        if (printable) {
            uint32_t x = rng() % (sizeof(CCH) - 1);
            ret[i] = CCH[x];
        } else {
            ret[i] = rng() % 0xFF;
        }
    }
    return ret;
}

bool is_safe(uint8_t b) {
    return b >= ' ' && b < 128;
}

string hexdump(const void *buf, size_t len) {
    string ret("\r\n");
    char tmp[8];
    const uint8_t *data = (const uint8_t *) buf;
    for (size_t i = 0; i < len; i += 16) {
        for (int j = 0; j < 16; ++j) {
            if (i + j < len) {
                int sz = snprintf(tmp, sizeof(tmp), "%.2x ", data[i + j]);
                ret.append(tmp, sz);
            } else {
                int sz = snprintf(tmp, sizeof(tmp), "   ");
                ret.append(tmp, sz);
            }
        }
        for (int j = 0; j < 16; ++j) {
            if (i + j < len) {
                ret += (is_safe(data[i + j]) ? data[i + j] : '.');
            } else {
                ret += (' ');
            }
        }
        ret += ('\n');
    }
    return ret;
}

string hexmem(const void *buf, size_t len) {
    string ret;
    char tmp[8];
    const uint8_t *data = (const uint8_t *) buf;
    for (size_t i = 0; i < len; ++i) {
        int sz = sprintf(tmp, "%.2x ", data[i]);
        ret.append(tmp, sz);
    }
    return ret;
}

string exePath(bool isExe /*= true*/) {
    char buffer[PATH_MAX * 2 + 1] = {0};
    int n = -1;
#if defined(_WIN32)
    n = GetModuleFileNameA(isExe?nullptr:(HINSTANCE)&__ImageBase, buffer, sizeof(buffer));
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
    } else {
        filePath = buffer;
    }

#if defined(_WIN32)
    //windows下把路径统一转换层unix风格，因为后续都是按照unix风格处理的  [AUTO-TRANSLATED:33d86ad3]
    //Convert paths to Unix style under Windows, as subsequent processing is done in Unix style
    for (auto &ch : filePath) {
        if (ch == '\\') {
            ch = '/';
        }
    }
#endif //defined(_WIN32)

    return filePath;
}

string exeDir(bool isExe /*= true*/) {
    auto path = exePath(isExe);
    return path.substr(0, path.rfind('/') + 1);
}

string exeName(bool isExe /*= true*/) {
    auto path = exePath(isExe);
    return path.substr(path.rfind('/') + 1);
}

// string转小写  [AUTO-TRANSLATED:bf92618b]
//Convert string to lowercase
std::string &strToLower(std::string &str) {
    transform(str.begin(), str.end(), str.begin(), towlower);
    return str;
}

// string转大写  [AUTO-TRANSLATED:0197b884]
//Convert string to uppercase
std::string &strToUpper(std::string &str) {
    transform(str.begin(), str.end(), str.begin(), towupper);
    return str;
}

// string转小写  [AUTO-TRANSLATED:bf92618b]
//Convert string to lowercase
std::string strToLower(std::string &&str) {
    transform(str.begin(), str.end(), str.begin(), towlower);
    return std::move(str);
}

// string转大写  [AUTO-TRANSLATED:0197b884]
//Convert string to uppercase
std::string strToUpper(std::string &&str) {
    transform(str.begin(), str.end(), str.begin(), towupper);
    return std::move(str);
}

vector<string> split(const string &s, const char *delim) {
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

#define TRIM(s, chars) \
do{ \
    string map(0xFF, '\0'); \
    for (auto &ch : chars) { \
        map[(unsigned char &)ch] = '\1'; \
    } \
    while( s.size() && map.at((unsigned char &)s.back())) s.pop_back(); \
    while( s.size() && map.at((unsigned char &)s.front())) s.erase(0,1); \
}while(0);

//去除前后的空格、回车符、制表符  [AUTO-TRANSLATED:0b0a7fc7]
//Remove leading and trailing spaces, carriage returns, and tabs
std::string &trim(std::string &s, const string &chars) {
    TRIM(s, chars);
    return s;
}

std::string trim(std::string &&s, const string &chars) {
    TRIM(s, chars);
    return std::move(s);
}

void replace(string &str, const string &old_str, const string &new_str,std::string::size_type b_pos) {
    if (old_str.empty() || old_str == new_str) {
        return;
    }
    auto pos = str.find(old_str,b_pos);
    if (pos == string::npos) {
        return;
    }
    str.replace(pos, old_str.size(), new_str);
    replace(str, old_str, new_str,pos + new_str.length());
}

bool start_with(const string &str, const string &substr) {
    return str.find(substr) == 0;
}

bool end_with(const string &str, const string &substr) {
    auto pos = str.rfind(substr);
    return pos != string::npos && pos == str.size() - substr.size();
}

bool isIP(const char *str) {
    return SockUtil::is_ipv4(str) || SockUtil::is_ipv6(str);
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

static long s_gmtoff = 0; //时间差
static onceToken s_token([]() {
#ifdef _WIN32
    TIME_ZONE_INFORMATION tzinfo;
    DWORD dwStandardDaylight;
    long bias;
    dwStandardDaylight = GetTimeZoneInformation(&tzinfo);
    bias = tzinfo.Bias;
    if (dwStandardDaylight == TIME_ZONE_ID_STANDARD) {
        bias += tzinfo.StandardBias;
    }
    if (dwStandardDaylight == TIME_ZONE_ID_DAYLIGHT) {
        bias += tzinfo.DaylightBias;
    }
    s_gmtoff = -bias * 60; //时间差(分钟)
#else
    local_time_init();
    s_gmtoff = getLocalTime(time(nullptr)).tm_gmtoff;
#endif // _WIN32
});

long getGMTOff() {
    return s_gmtoff;
}

static inline uint64_t getCurrentMicrosecondOrigin() {
#if !defined(_WIN32)
    struct timeval tv;
    gettimeofday(&tv, nullptr);
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
        DebugL << "Stamp thread started";
        uint64_t last = getCurrentMicrosecondOrigin();
        uint64_t now;
        uint64_t microsecond = 0;
        while (true) {
            now = getCurrentMicrosecondOrigin();
            //记录系统时间戳，可回退  [AUTO-TRANSLATED:495a0114]
            //Record system timestamp, can be rolled back
            s_currentMicrosecond_system.store(now, memory_order_release);
            s_currentMillisecond_system.store(now / 1000, memory_order_release);

            //记录流逝时间戳，不可回退  [AUTO-TRANSLATED:7f3a9da3]
            //Record elapsed timestamp, cannot be rolled back
            int64_t expired = now - last;
            last = now;
            if (expired > 0 && expired < 1000 * 1000) {
                //流逝时间处于0~1000ms之间，那么是合理的，说明没有调整系统时间  [AUTO-TRANSLATED:566e1001]
                //If the elapsed time is between 0~1000ms, it is reasonable, indicating that the system time has not been adjusted
                microsecond += expired;
                s_currentMicrosecond.store(microsecond, memory_order_release);
                s_currentMillisecond.store(microsecond / 1000, memory_order_release);
            } else if (expired != 0) {
                WarnL << "Stamp expired is abnormal: " << expired;
            }
            //休眠0.5 ms  [AUTO-TRANSLATED:5e20acdd]
            //Sleep for 0.5 ms
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
    if (system_time) {
        return s_currentMillisecond_system.load(memory_order_acquire);
    }
    return s_currentMillisecond.load(memory_order_acquire);
}

uint64_t getCurrentMicrosecond(bool system_time) {
    static bool flag = initMillisecondThread();
    if (system_time) {
        return s_currentMicrosecond_system.load(memory_order_acquire);
    }
    return s_currentMicrosecond.load(memory_order_acquire);
}

string getTimeStr(const char *fmt, time_t time) {
    if (!time) {
        time = ::time(nullptr);
    }
    auto tm = getLocalTime(time);
    size_t size = strlen(fmt) + 64;
    string ret;
    ret.resize(size);
    size = std::strftime(&ret[0], size, fmt, &tm);
    if (size > 0) {
        ret.resize(size);
    }
    else{
        ret = fmt;
    }
    return ret;
}


struct tm getLocalTime(time_t sec) {
    struct tm tm;
#ifdef _WIN32
    localtime_s(&tm, &sec);
#else
    no_locks_localtime(&tm, sec);
#endif //_WIN32
    return tm;
}

static thread_local string thread_name;

static string limitString(const char *name, size_t max_size) {
    string str = name;
    if (str.size() + 1 > max_size) {
        auto erased = str.size() + 1 - max_size + 3;
        str.replace(5, erased, "...");
    }
    return str;
}

void setThreadName(const char *name) {
    assert(name);
#if defined(__linux) || defined(__linux__) || defined(__MINGW32__)
    pthread_setname_np(pthread_self(), limitString(name, 16).data());
#elif defined(__MACH__) || defined(__APPLE__)
    pthread_setname_np(limitString(name, 32).data());
#elif defined(_MSC_VER)
    // SetThreadDescription was added in 1607 (aka RS1). Since we can't guarantee the user is running 1607 or later, we need to ask for the function from the kernel.
    using SetThreadDescriptionFunc = HRESULT(WINAPI * )(_In_ HANDLE hThread, _In_ PCWSTR lpThreadDescription);
    static auto setThreadDescription = reinterpret_cast<SetThreadDescriptionFunc>(::GetProcAddress(::GetModuleHandle("Kernel32.dll"), "SetThreadDescription"));
    if (setThreadDescription) {
        // Convert the thread name to Unicode
        wchar_t threadNameW[MAX_PATH];
        size_t numCharsConverted;
        errno_t wcharResult = mbstowcs_s(&numCharsConverted, threadNameW, name, MAX_PATH - 1);
        if (wcharResult == 0) {
            HRESULT hr = setThreadDescription(::GetCurrentThread(), threadNameW);
            if (!SUCCEEDED(hr)) {
                int i = 0;
                i++;
            }
        }
    } else {
        // For understanding the types and values used here, please see:
        // https://docs.microsoft.com/en-us/visualstudio/debugger/how-to-set-a-thread-name-in-native-code

        const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push, 8)
        struct THREADNAME_INFO {
            DWORD dwType = 0x1000; // Must be 0x1000
            LPCSTR szName;         // Pointer to name (in user address space)
            DWORD dwThreadID;      // Thread ID (-1 for caller thread)
            DWORD dwFlags = 0;     // Reserved for future use; must be zero
        };
#pragma pack(pop)

        THREADNAME_INFO info;
        info.szName = name;
        info.dwThreadID = (DWORD) - 1;

        __try{
                RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), reinterpret_cast<const ULONG_PTR *>(&info));
        } __except(GetExceptionCode() == MS_VC_EXCEPTION ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_EXECUTE_HANDLER) {
        }
    }
#else
    thread_name = name ? name : "";
#endif
}

string getThreadName() {
#if ((defined(__linux) || defined(__linux__)) && !defined(ANDROID)) || (defined(__MACH__) || defined(__APPLE__)) || (defined(ANDROID) && __ANDROID_API__ >= 26) || defined(__MINGW32__)
    string ret;
    ret.resize(32);
    auto tid = pthread_self();
    pthread_getname_np(tid, (char *) ret.data(), ret.size());
    if (ret[0]) {
        ret.resize(strlen(ret.data()));
        return ret;
    }
    return to_string((uint64_t) tid);
#elif defined(_MSC_VER)
    using GetThreadDescriptionFunc = HRESULT(WINAPI * )(_In_ HANDLE hThread, _In_ PWSTR * ppszThreadDescription);
    static auto getThreadDescription = reinterpret_cast<GetThreadDescriptionFunc>(::GetProcAddress(::GetModuleHandleA("Kernel32.dll"), "GetThreadDescription"));

    if (!getThreadDescription) {
        std::ostringstream ss;
        ss << std::this_thread::get_id();
        return ss.str();
    } else {
        PWSTR data;
        HRESULT hr = getThreadDescription(GetCurrentThread(), &data);
        if (SUCCEEDED(hr) && data[0] != '\0') {
            char threadName[MAX_PATH];
            size_t numCharsConverted;
            errno_t charResult = wcstombs_s(&numCharsConverted, threadName, data, MAX_PATH - 1);
            if (charResult == 0) {
                LocalFree(data);
                std::ostringstream ss;
                ss << threadName;
                return ss.str();
            } else {
                if (data) {
                    LocalFree(data);
                }
                return to_string((uint64_t) GetCurrentThreadId());
            }
        } else {
            if (data) {
                LocalFree(data);
            }
            return to_string((uint64_t) GetCurrentThreadId());
        }
    }
#else
    if (!thread_name.empty()) {
        return thread_name;
    }
    std::ostringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
#endif
}

bool setThreadAffinity(int i) {
#if (defined(__linux) || defined(__linux__)) && !defined(ANDROID)
    cpu_set_t mask;
    CPU_ZERO(&mask);
    if (i >= 0) {
        CPU_SET(i, &mask);
    } else {
        for (auto j = 0u; j < thread::hardware_concurrency(); ++j) {
            CPU_SET(j, &mask);
        }
    }
    if (!pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask)) {
        return true;
    }
    WarnL << "pthread_setaffinity_np failed: " << get_uv_errmsg();
#endif
    return false;
}

// Demangle a mangled symbol name and return the demangled name.
// If 'mangled' isn't mangled in the first place, this function
// simply returns 'mangled' as is.
//
// This function is used for demangling mangled symbol names such as
// '_Z3bazifdPv'.  It uses abi::__cxa_demangle() if your compiler has
// the API.  Otherwise, this function simply returns 'mangled' as is.
//
// Currently, we support only GCC 3.4.x or later for the following
// reasons.
//
// - GCC 2.95.3 doesn't have cxxabi.h
// - GCC 3.3.5 and ICC 9.0 have a bug.  Their abi::__cxa_demangle()
//   returns junk values for non-mangled symbol names (ex. function
//   names in C linkage).  For example,
//     abi::__cxa_demangle("main", 0,  0, &status)
//   returns "unsigned long" and the status code is 0 (successful).
//
// Also,
//
//  - MIPS is not supported because abi::__cxa_demangle() is not defined.
//  - Android x86 is not supported because STLs don't define __cxa_demangle
//
string demangle(const char *mangled) {
    int status = 0;
    char *demangled = nullptr;
#if HAS_CXA_DEMANGLE
    demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
#endif
    string out;
    if (status == 0 && demangled) { // Demangling succeeeded.
        out.append(demangled);
#ifdef ASAN_USE_DELETE
        delete [] demangled; // 开启asan后，用free会卡死
#else
        free(demangled);
#endif
    } else {
        out.append(mangled);
    }
    return out;
}

string getEnv(const string &key) {
    auto ekey = key.c_str();
    if (*ekey == '$') {
        ++ekey;
    }
    auto value = *ekey ? getenv(ekey) : nullptr;
    return value ? value : "";
}


void Creator::onDestoryException(const type_info &info, const exception &ex) {
    ErrorL << "Invoke " << demangle(info.name()) << "::onDestory throw a exception: " << ex.what();
}

}  // namespace toolkit


extern "C" {
void Assert_Throw(int failed, const char *exp, const char *func, const char *file, int line, const char *str) {
    if (failed) {
        toolkit::_StrPrinter printer;
        printer << "Assertion failed: (" << exp ;
        if(str && *str){
            printer << ", " << str;
        }
        printer << "), function " << func << ", file " << file << ", line " << line << ".";
        throw toolkit::AssertFailedException(printer);
    }
}
}