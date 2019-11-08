/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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

#ifndef UTIL_UTIL_H_
#define UTIL_UTIL_H_

#if defined(_WIN32)
#include <WinSock2.h>
#pragma comment (lib,"WS2_32")
#else
#include <unistd.h>
#include <sys/time.h>
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
    static class_name &s_insteanc_ref = *s_instance; \
    return s_insteanc_ref; \
}

#include <stdio.h>
#include <string.h>
#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>

using namespace std;

namespace toolkit {

#define StrPrinter _StrPrinter()
class _StrPrinter : public string {
public:
    _StrPrinter() {}

    template<typename T>
    _StrPrinter& operator <<(T && data) {
        _stream << std::forward<T>(data);
        this->string::operator=(_stream.str());
        return *this;
    }

    string operator <<(std::ostream&(*f)(std::ostream&)) const {
        return *this;
    }

private:
    stringstream _stream;
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
    typedef std::shared_ptr<Any> Ptr;

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
class AnyStorage : public unordered_map<string,Any>{
public:
    AnyStorage() = default;
    ~AnyStorage() = default;
    typedef std::shared_ptr<AnyStorage> Ptr;
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

string makeRandStr(int sz, bool printable = true);
string hexdump(const void *buf, size_t len);
string exePath();
string exeDir();
string exeName();

vector<string> split(const string& s, const char *delim);
//去除前后的空格、回车符、制表符...
std::string& trim(std::string &s,const string &chars=" \r\n\t");
std::string trim(std::string &&s,const string &chars=" \r\n\t");
// string转小写
std::string &strToLower(std::string &str);
std::string strToLower(std::string &&str);
// string转大写
std::string &strToUpper(std::string &str);
std::string strToUpper(std::string &&str);
void replace(string &str, const string &old_str, const string &new_str) ;
bool isIP(const char *str);

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
int asprintf(char **strp, const char *fmt, ...);
#if !defined(strcasecmp)
#define strcasecmp _stricmp
#endif

#endif //WIN32


/**
 * 获取1970年至今的毫秒数
 * @return
 */
uint64_t getCurrentMillisecond();


/**
 * 获取1970年至今的微秒数
 * @return
 */
uint64_t getCurrentMicrosecond();

}  // namespace toolkit

#endif /* UTIL_UTIL_H_ */
