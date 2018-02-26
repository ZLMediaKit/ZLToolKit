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

#ifndef UTIL_UTIL_H_
#define UTIL_UTIL_H_

#if defined(_WIN32)
#include <WinSock2.h>
#pragma comment (lib,"WS2_32")
#else
#include <unistd.h>
#include <sys/time.h>
#endif // defined(_WIN32)

#include <stdio.h>
#include <string.h>
#include <string>
#include <sstream>
#include <vector>

using namespace std;

namespace ZL {
namespace Util {

class _StrPrinter {
public:
    _StrPrinter() {
    }
    template<typename T>
    _StrPrinter& operator <<(const T& data) {
        ss << data;
        return *this;
    }
    operator string (){
        return ss.str();
    }
    string operator <<(std::ostream&(*f)(std::ostream&)) const {
        return ss.str();
    }
private:
    stringstream ss;
};

#define StrPrinter _StrPrinter()

string makeRandStr(int sz, bool printable = true);
string hexdump(const void *buf, size_t len);
string exePath();
string exeDir();
string exeName();
//设置应用程序文件路径，非win/mac/linux系统才有效
void setExePath(const string &path);

vector<string> split(const string& s, const char *delim);
//去除前后的空格、回车符、制表符...
std::string& trim(std::string &s,const string &chars=" \r\n\t");
std::string trim(std::string &&s,const string &chars=" \r\n\t");
// string转小写
std::string &strToLower(std::string &str);
// string转大写
std::string &strToUpper(std::string &str);


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

#if !defined(strcasecmp)
int strcasecmp(const char *strA,const char *strB);
#endif //!defined(strcasecmp)

void usleep(int micro_seconds);
void sleep(int second);

#endif //WIN32

}  // namespace Util
}  // namespace ZL

#endif /* UTIL_UTIL_H_ */
