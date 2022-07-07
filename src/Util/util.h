/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef UTIL_UTIL_H_
#define UTIL_UTIL_H_

#include <ctime>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include <atomic>
#include <unordered_map>
#if defined(_WIN32)
#undef FD_SETSIZE
//修改默认64为1024路
#define FD_SETSIZE 1024
#include <winsock2.h>
#pragma comment (lib,"WS2_32")
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <cstddef>
#endif // defined(_WIN32)

#if defined(__APPLE__)
#include "TargetConditionals.h"
#if TARGET_IPHONE_SIMULATOR
#define OS_IPHONE
#elif TARGET_OS_IPHONE
#define OS_IPHONE
#endif
#endif //__APPLE__

#define INSTANCE_IMP(class_name, ...) \
class_name &class_name::Instance() { \
    static std::shared_ptr<class_name> s_instance(new class_name(__VA_ARGS__)); \
    static class_name &s_instance_ref = *s_instance; \
    return s_instance_ref; \
}

namespace toolkit {

#define StrPrinter ::toolkit::_StrPrinter()
class _StrPrinter : public std::string {
public:
    _StrPrinter() {}

    template<typename T>
    _StrPrinter& operator <<(T && data) {
        _stream << std::forward<T>(data);
        this->std::string::operator=(_stream.str());
        return *this;
    }

    std::string operator <<(std::ostream&(*f)(std::ostream&)) const {
        return *this;
    }

private:
    std::stringstream _stream;
};

//禁止拷贝基类
class noncopyable {
protected:
    noncopyable() {}
    ~noncopyable() {}
private:
    //禁止拷贝
    noncopyable(const noncopyable &that) = delete;
    noncopyable(noncopyable &&that) = delete;
    noncopyable &operator=(const noncopyable &that) = delete;
    noncopyable &operator=(noncopyable &&that) = delete;
};

//可以保存任意的对象
class Any{
public:
    using Ptr = std::shared_ptr<Any>;

    Any() = default;
    ~Any() = default;

    template <typename C,typename ...ArgsType>
    void set(ArgsType &&...args){
        _data.reset(new C(std::forward<ArgsType>(args)...),[](void *ptr){
            delete (C*) ptr;
        });
    }
    template <typename C>
    C& get(){
        if(!_data){
            throw std::invalid_argument("Any is empty");
        }
        C *ptr = (C *)_data.get();
        return *ptr;
    }

    operator bool() {
        return _data.operator bool ();
    }
    bool empty(){
        return !bool();
    }
private:
    std::shared_ptr<void> _data;
};

//用于保存一些外加属性
class AnyStorage : public std::unordered_map<std::string,Any>{
public:
    AnyStorage() = default;
    ~AnyStorage() = default;
    using Ptr = std::shared_ptr<AnyStorage>;
};

//对象安全的构建和析构
//构建后执行onCreate函数
//析构前执行onDestory函数
//在函数onCreate和onDestory中可以执行构造或析构中不能调用的方法，比如说shared_from_this或者虚函数
class Creator {
public:
    template<typename C,typename ...ArgsType>
    static std::shared_ptr<C> create(ArgsType &&...args){
        std::shared_ptr<C> ret(new C(std::forward<ArgsType>(args)...),[](C *ptr){
            ptr->onDestory();
            delete ptr;
        });
        ret->onCreate();
        return ret;
    }
private:
    Creator() = default;
    ~Creator() = default;
};


template <class C>
class ObjectStatistic{
public:
    ObjectStatistic(){
        ++getCounter();
    }

    ~ObjectStatistic(){
        --getCounter();
    }

    static size_t count(){
        return getCounter().load();
    }

private:
    static std::atomic<size_t> & getCounter();
};

#define StatisticImp(Type)  \
    template<> \
    std::atomic<size_t>& ObjectStatistic<Type>::getCounter(){ \
        static std::atomic<size_t> instance(0); \
        return instance; \
    }

std::string makeRandStr(int sz, bool printable = true);
std::string hexdump(const void *buf, size_t len);
std::string hexmem(const void* buf, size_t len);
std::string exePath(bool isExe = true);
std::string exeDir(bool isExe = true);
std::string exeName(bool isExe = true);

std::vector<std::string> split(const std::string& s, const char *delim);
//去除前后的空格、回车符、制表符...
std::string& trim(std::string &s,const std::string &chars=" \r\n\t");
std::string trim(std::string &&s,const std::string &chars=" \r\n\t");
// string转小写
std::string &strToLower(std::string &str);
std::string strToLower(std::string &&str);
// string转大写
std::string &strToUpper(std::string &str);
std::string strToUpper(std::string &&str);
//替换子字符串
void replace(std::string &str, const std::string &old_str, const std::string &new_str) ;
//判断是否为ip
bool isIP(const char *str);
//字符串是否以xx开头
bool start_with(const std::string &str, const std::string &substr);
//字符串是否以xx结尾
bool end_with(const std::string &str, const std::string &substr);

#ifndef bzero
#define bzero(ptr,size)  memset((ptr),0,(size));
#endif //bzero

#if defined(ANDROID)
template <typename T>
std::string to_string(T value){
    std::ostringstream os ;
    os <<  std::forward<T>(value);
    return os.str() ;
}
#endif//ANDROID

#if defined(_WIN32)
int gettimeofday(struct timeval *tp, void *tzp);
void usleep(int micro_seconds);
void sleep(int second);
int vasprintf(char **strp, const char *fmt, va_list ap);
int asprintf(char **strp, const char *fmt, ...);
const char *strcasestr(const char *big, const char *little);

#if !defined(strcasecmp)
    #define strcasecmp _stricmp
#endif

#ifndef ssize_t
    #ifdef _WIN64
        #define ssize_t int64_t
    #else
        #define ssize_t int32_t
    #endif
#endif
#endif //WIN32

/**
 * 获取时间差, 返回值单位为秒
 */
long getGMTOff();

/**
 * 获取1970年至今的毫秒数
 * @param system_time 是否为系统时间(系统时间可以回退),否则为程序启动时间(不可回退)
 */
uint64_t getCurrentMillisecond(bool system_time = false);

/**
 * 获取1970年至今的微秒数
 * @param system_time 是否为系统时间(系统时间可以回退),否则为程序启动时间(不可回退)
 */
uint64_t getCurrentMicrosecond(bool system_time = false);

/**
 * 获取时间字符串
 * @param fmt 时间格式，譬如%Y-%m-%d %H:%M:%S
 * @return 时间字符串
 */
std::string getTimeStr(const char *fmt,time_t time = 0);

/**
 * 根据unix时间戳获取本地时间
 * @param sec unix时间戳
 * @return tm结构体
 */
struct tm getLocalTime(time_t sec);

/**
 * 设置线程名
 */
void setThreadName(const char *name);

/**
 * 获取线程名
 */
std::string getThreadName();

/**
 * 设置当前线程cpu亲和性
 * @param i cpu索引，如果为-1，那么取消cpu亲和性
 * @return 是否成功，目前只支持linux
 */
bool setThreadAffinity(int i);

/**
 * 根据typeid(class).name()获取类名
 */
std::string demangle(const char *mangled);

/**
 * 获取环境变量内容，以'$'开头
 */
std::string getEnv(const std::string &key);

}  // namespace toolkit
#endif /* UTIL_UTIL_H_ */
