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
#include "function_traits.h"
#include "onceToken.h"
#if defined(_WIN32)
#undef FD_SETSIZE
//修改默认64为1024路  [AUTO-TRANSLATED:90567e14]
//Modify the default 64 to 1024 paths
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

//禁止拷贝基类  [AUTO-TRANSLATED:a4ca4dcb]
//Prohibit copying of base classes
class noncopyable {
protected:
    noncopyable() {}
    ~noncopyable() {}
private:
    //禁止拷贝  [AUTO-TRANSLATED:e8af72e3]
    //Prohibit copying
    noncopyable(const noncopyable &that) = delete;
    noncopyable(noncopyable &&that) = delete;
    noncopyable &operator=(const noncopyable &that) = delete;
    noncopyable &operator=(noncopyable &&that) = delete;
};

#ifndef CLASS_FUNC_TRAITS
#define CLASS_FUNC_TRAITS(func_name) \
template<typename T, typename ... ARGS> \
constexpr bool Has_##func_name(decltype(&T::on##func_name) /*unused*/) { \
    using RET = typename function_traits<decltype(&T::on##func_name)>::return_type; \
    using FuncType = RET (T::*)(ARGS...);   \
    return std::is_same<decltype(&T::on ## func_name), FuncType>::value; \
} \
\
template<class T, typename ... ARGS> \
constexpr bool Has_##func_name(...) { \
    return false; \
} \
\
template<typename T, typename ... ARGS> \
static void InvokeFunc_##func_name(typename std::enable_if<!Has_##func_name<T, ARGS...>(nullptr), T>::type &obj, ARGS ...args) {} \
\
template<typename T, typename ... ARGS>\
static typename function_traits<decltype(&T::on##func_name)>::return_type InvokeFunc_##func_name(typename std::enable_if<Has_##func_name<T, ARGS...>(nullptr), T>::type &obj, ARGS ...args) {\
    return obj.on##func_name(std::forward<ARGS>(args)...);\
}
#endif //CLASS_FUNC_TRAITS

#ifndef CLASS_FUNC_INVOKE
#define CLASS_FUNC_INVOKE(T, obj, func_name, ...) InvokeFunc_##func_name<T>(obj, ##__VA_ARGS__)
#endif //CLASS_FUNC_INVOKE

CLASS_FUNC_TRAITS(Destory)
CLASS_FUNC_TRAITS(Create)

/**
 * 对象安全的构建和析构,构建后执行onCreate函数,析构前执行onDestory函数
 * 在函数onCreate和onDestory中可以执行构造或析构中不能调用的方法，比如说shared_from_this或者虚函数
 * @warning onDestory函数确保参数个数为0；否则会被忽略调用
 * Object-safe construction and destruction, execute the onCreate function after construction, and execute the onDestroy function before destruction
 * Methods that cannot be called during construction or destruction, such as shared_from_this or virtual functions, can be executed in the onCreate and onDestroy functions
 * @warning The onDestroy function must have 0 parameters; otherwise, it will be ignored
 
 * [AUTO-TRANSLATED:54ef34ac]
 */
class Creator {
public:
    /**
     * 创建对象，用空参数执行onCreate和onDestory函数
     * @param args 对象构造函数参数列表
     * @return args对象的智能指针
     * Create an object, execute onCreate and onDestroy functions with empty parameters
     * @param args List of parameters for the object's constructor
     * @return Smart pointer to the args object
     
     * [AUTO-TRANSLATED:c6c90c2b]
     */
    template<typename C, typename ...ArgsType>
    static std::shared_ptr<C> create(ArgsType &&...args) {
        std::shared_ptr<C> ret(new C(std::forward<ArgsType>(args)...), [](C *ptr) {
            try {
                CLASS_FUNC_INVOKE(C, *ptr, Destory);
            } catch (std::exception &ex){
                onDestoryException(typeid(C), ex);
            }
            delete ptr;
        });
        CLASS_FUNC_INVOKE(C, *ret, Create);
        return ret;
    }

    /**
     * 创建对象，用指定参数执行onCreate函数
     * @param args 对象onCreate函数参数列表
     * @warning args参数类型和个数必须与onCreate函数类型匹配(不可忽略默认参数)，否则会由于模板匹配失败导致忽略调用
     * @return args对象的智能指针
     * Create an object, execute the onCreate function with specified parameters
     * @param args List of parameters for the object's onCreate function
     * @warning The type and number of args parameters must match the type of the onCreate function (default parameters cannot be ignored), otherwise it will be ignored due to template matching failure
     * @return Smart pointer to the args object
     
     * [AUTO-TRANSLATED:bd672150]
     */
    template<typename C, typename ...ArgsType>
    static std::shared_ptr<C> create2(ArgsType &&...args) {
        std::shared_ptr<C> ret(new C(), [](C *ptr) {
            try {
                CLASS_FUNC_INVOKE(C, *ptr, Destory);
            } catch (std::exception &ex){
                onDestoryException(typeid(C), ex);
            }
            delete ptr;
        });
        CLASS_FUNC_INVOKE(C, *ret, Create, std::forward<ArgsType>(args)...);
        return ret;
    }

private:
    static void onDestoryException(const std::type_info &info, const std::exception &ex);

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

class AssertFailedException : public std::runtime_error {
public:
    template<typename ...T>
    AssertFailedException(T && ...args) : std::runtime_error(std::forward<T>(args)...) {}
};

std::string makeRandStr(int sz, bool printable = true);
std::string hexdump(const void *buf, size_t len);
std::string hexmem(const void* buf, size_t len);
std::string exePath(bool isExe = true);
std::string exeDir(bool isExe = true);
std::string exeName(bool isExe = true);

std::vector<std::string> split(const std::string& s, const char *delim);
//去除前后的空格、回车符、制表符...  [AUTO-TRANSLATED:7c50cbc8]
//Remove leading and trailing spaces, line breaks, tabs...
std::string& trim(std::string &s,const std::string &chars=" \r\n\t");
std::string trim(std::string &&s,const std::string &chars=" \r\n\t");
// string转小写  [AUTO-TRANSLATED:bf92618b]
//Convert string to lowercase
std::string &strToLower(std::string &str);
std::string strToLower(std::string &&str);
// string转大写  [AUTO-TRANSLATED:0197b884]
//Convert string to uppercase
std::string &strToUpper(std::string &str);
std::string strToUpper(std::string &&str);
//替换子字符串  [AUTO-TRANSLATED:cbacb116]
//Replace substring
void replace(std::string &str, const std::string &old_str, const std::string &new_str, std::string::size_type b_pos = 0) ;
//判断是否为ip  [AUTO-TRANSLATED:288e7a54]
//Determine if it's an IP
bool isIP(const char *str);
//字符串是否以xx开头  [AUTO-TRANSLATED:585cf826]
//Check if a string starts with xx
bool start_with(const std::string &str, const std::string &substr);
//字符串是否以xx结尾  [AUTO-TRANSLATED:50cc80d7]
//Check if a string ends with xx
bool end_with(const std::string &str, const std::string &substr);
//拼接格式字符串  [AUTO-TRANSLATED:2f902ef7]
//Concatenate format string
template<typename... Args>
std::string str_format(const std::string &format, Args... args) {

    // Calculate the buffer size
    auto size_buf = snprintf(nullptr, 0, format.c_str(), args ...) + 1;
    // Allocate the buffer
#if __cplusplus >= 201703L
    // C++17
    auto buf = std::make_unique<char[]>(size_buf);
#else
    // C++11
    std:: unique_ptr<char[]> buf(new(std::nothrow) char[size_buf]);
#endif
    // Check if the allocation is successful
    if (buf == nullptr) {
        return {};
    }
    // Fill the buffer with formatted string
    auto result = snprintf(buf.get(), size_buf, format.c_str(), args ...);
    // Return the formatted string
    return std::string(buf.get(), buf.get() + result);
}

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

#if !defined(strncasecmp)
#define strncasecmp _strnicmp
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
 * Get time difference, return value in seconds
 
 * [AUTO-TRANSLATED:43d2403a]
 */
long getGMTOff();

/**
 * 获取1970年至今的毫秒数
 * @param system_time 是否为系统时间(系统时间可以回退),否则为程序启动时间(不可回退)
 * Get the number of milliseconds since 1970
 * @param system_time Whether it's system time (system time can be rolled back), otherwise it's program startup time (cannot be rolled back)
 
 * [AUTO-TRANSLATED:9857bfbe]
 */
uint64_t getCurrentMillisecond(bool system_time = false);

/**
 * 获取1970年至今的微秒数
 * @param system_time 是否为系统时间(系统时间可以回退),否则为程序启动时间(不可回退)
 * Get the number of microseconds since 1970
 * @param system_time Whether it's system time (system time can be rolled back), otherwise it's program startup time (cannot be rolled back)
 
 * [AUTO-TRANSLATED:e4bed7e3]
 */
uint64_t getCurrentMicrosecond(bool system_time = false);

/**
 * 获取时间字符串
 * @param fmt 时间格式，譬如%Y-%m-%d %H:%M:%S
 * @return 时间字符串
 * Get time string
 * @param fmt Time format, e.g. %Y-%m-%d %H:%M:%S
 * @return Time string
 
 * [AUTO-TRANSLATED:444636ec]
 */
std::string getTimeStr(const char *fmt,time_t time = 0);

/**
 * 根据unix时间戳获取本地时间
 * @param sec unix时间戳
 * @return tm结构体
 * Get local time based on Unix timestamp
 * @param sec Unix timestamp
 * @return tm structure
 
 * [AUTO-TRANSLATED:22a03a5b]
 */
struct tm getLocalTime(time_t sec);

/**
 * 设置线程名
 * Set thread name
 
 * [AUTO-TRANSLATED:d0bcbcdc]
 */
void setThreadName(const char *name);

/**
 * 获取线程名
 * Get thread name
 
 * [AUTO-TRANSLATED:99245fec]
 */
std::string getThreadName();

/**
 * 设置当前线程cpu亲和性
 * @param i cpu索引，如果为-1，那么取消cpu亲和性
 * @return 是否成功，目前只支持linux
 * Set current thread CPU affinity
 * @param i CPU index, if -1, cancel CPU affinity
 * @return Whether successful, currently only supports Linux
 
 * [AUTO-TRANSLATED:9b3d6a83]
 */
bool setThreadAffinity(int i);

/**
 * 根据typeid(class).name()获取类名
 * Get class name based on typeid(class).name()
 
 * [AUTO-TRANSLATED:7ac66c58]
 */
std::string demangle(const char *mangled);

/**
 * 获取环境变量内容，以'$'开头
 * Get environment variable content, starting with '$'
 
 * [AUTO-TRANSLATED:c2c1689d]
 */
std::string getEnv(const std::string &key);

// 可以保存任意的对象  [AUTO-TRANSLATED:e7c40bad]
//Can store any object
class Any {
public:
    using Ptr = std::shared_ptr<Any>;

    Any() = default;
    ~Any() = default;

    Any(const Any &that) = default;
    Any(Any &&that) {
        _type = that._type;
        _data = std::move(that._data);
    }

    Any &operator=(const Any &that) = default;
    Any &operator=(Any &&that) {
        _type = that._type;
        _data = std::move(that._data);
        return *this;
    }

    template <typename T, typename... ArgsType>
    void set(ArgsType &&...args) {
        _type = &typeid(T);
        _data.reset(new T(std::forward<ArgsType>(args)...), [](void *ptr) { delete (T *)ptr; });
    }

    template <typename T>
    void set(std::shared_ptr<T> data) {
        if (data) {
            _type = &typeid(T);
            _data = std::move(data);
        } else {
            reset();
        }
    }

    template <typename T>
    T &get(bool safe = true) {
        if (!_data) {
            throw std::invalid_argument("Any is empty");
        }
        if (safe && !is<T>()) {
            throw std::invalid_argument("Any::get(): " + demangle(_type->name()) + " unable cast to " + demangle(typeid(T).name()));
        }
        return *((T *)_data.get());
    }

    template <typename T>
    const T &get(bool safe = true) const {
        return const_cast<Any &>(*this).get<T>(safe);
    }

    template <typename T>
    bool is() const {
        return _type && typeid(T) == *_type;
    }

    operator bool() const { return _data.operator bool(); }
    bool empty() const { return !operator bool(); }

    void reset() {
        _type = nullptr;
        _data = nullptr;
    }

    Any &operator=(std::nullptr_t) {
        reset();
        return *this;
    }

    std::string type_name() const {
        if (!_type) {
            return "";
        }
        return demangle(_type->name());
    }

private:
    const std::type_info* _type = nullptr;
    std::shared_ptr<void> _data;
};

// 用于保存一些外加属性  [AUTO-TRANSLATED:cfbc20a3]
//Used to store some additional properties
class AnyStorage : public std::unordered_map<std::string, Any> {
public:
    AnyStorage() = default;
    ~AnyStorage() = default;
    using Ptr = std::shared_ptr<AnyStorage>;
};

template <class R, class... ArgTypes>
class function_safe;

template <typename R, typename... ArgTypes>
class function_safe<R(ArgTypes...)> {
public:
    using func = std::function<R(ArgTypes...)>;
    using this_type = function_safe<R(ArgTypes...)>;

    template <class F>
    using enable_if_not_this = typename std::enable_if<!std::is_same<this_type, typename std::decay<F>::type>::value, typename std::decay<F>::type>::type;

    R operator()(ArgTypes... args) const {
        onceToken token([&]() { _doing = true; checkUpdate(); }, [&]() { checkUpdate(); _doing = false; });
        if (!_impl) {
            throw std::invalid_argument("try to invoke a empty functional");
        }
        return _impl(std::forward<ArgTypes>(args)...);
    }

    function_safe(std::nullptr_t) {
        update(func{});
    }

    function_safe &operator=(std::nullptr_t) {
        update(func{});
        return *this;
    }

    template <typename F, typename = enable_if_not_this<F>>
    function_safe(F &&f) {
        update(func { std::forward<F>(f) });
    }

    template <typename F, typename = enable_if_not_this<F>>
    this_type &operator=(F &&f) {
        update(func { std::forward<F>(f) });
        return *this;
    }

    function_safe() = default;
    function_safe(this_type &&) = default;
    function_safe(const this_type &) = default;
    this_type &operator=(this_type &&) = default;
    this_type &operator=(const this_type &) = default;

    operator bool() const { return _update ? (bool)_tmp : (bool)_impl; }

private:
    void checkUpdate() const {
        if (_update) {
            _update = false;
            _impl = std::move(_tmp);
        }
    }
    void update(func in) {
        if (!_doing) {
            // 没在执行中，那么立即覆写
            _impl = std::move(in);
            _tmp = nullptr;
            _update = false;
        } else {
            // 正在执行中，延后覆写
            _tmp = std::move(in);
            _update = true;
        }
    }

private:
    mutable bool _update = false;
    mutable bool _doing = false;
    mutable func _tmp;
    mutable func _impl;
};

}  // namespace toolkit

#ifdef __cplusplus
extern "C" {
#endif
extern void Assert_Throw(int failed, const char *exp, const char *func, const char *file, int line, const char *str);
#ifdef __cplusplus
}
#endif

#endif /* UTIL_UTIL_H_ */
