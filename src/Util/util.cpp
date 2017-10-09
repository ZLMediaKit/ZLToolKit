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

#include <stdlib.h>
#include <mutex>
#include <string>
#include <algorithm>
#include <unordered_map>

#include "util.h"
#include "Util/File.h"
#include "Util/logger.h"
#include "Util/uv_errno.h"

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

namespace ZL {
namespace Util {
string makeRandStr(int sz, bool printable) {
	char *tmp =  new char[sz+1];
	static const char CCH[] =
			"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int i;
	for (i = 0; i < sz; i++) {
		srand((unsigned) time(NULL) + i);
		if (printable) {
			int x = rand() % (sizeof(CCH) - 1);
			tmp[i] = CCH[x];
		} else {
			tmp[i] = rand() % 0xFF;
		}
	}
	tmp[i] = 0;
	string ret = tmp;
	delete [] tmp;
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
				int sz = sprintf(tmp, "%.2x ", data[i + j]);
				ret.append(tmp, sz);
			} else {
				int sz = sprintf(tmp, "   ");
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

static string _exePath("./");
string exePath() {
	char buffer[PATH_MAX * 2 + 1] = {0};
	int n = -1;
#if defined(_WIN32)
	n = GetModuleFileNameA(NULL, buffer, sizeof(buffer));
#elif defined(__MACH__) || defined(__APPLE__)
	n = sizeof(buffer);
   	if(uv_exepath(buffer,&n) !=0 ){
		n = -1;
	}
#elif defined(__linux__)
	n = readlink("/proc/self/exe", buffer, sizeof(buffer));
#endif

	string filePath;
	if (n <= 0) {
		filePath = _exePath;
	} else {
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
void setExePath(const string &path){
    _exePath=path;
}
string exeDir(){
	auto path = exePath();
	return path.substr(0, path.find_last_of('/') + 1);
}
string exeName(){
	auto path = exePath();
	return path.substr(path.find_last_of('/') + 1);
}
// string转小写
std::string  strToLower(const std::string &str)
{
    std::string strTmp = str;
    transform(strTmp.begin(), strTmp.end(), strTmp.begin(), towupper);
    return strTmp;
}



#if defined(_WIN32)

#if !defined(strcasecmp)
int strcasecmp(const char *strA,const char *strB){
	string str1 = strToLower(strA);
	string str2 = strToLower(strB);
	return str1.compare(str2);
}
#endif// !defined(strcasecmp)

void sleep(int second) {
	Sleep(1000 * second);
}
void usleep(int micro_seconds) {
	struct timeval tm;
	tm.tv_sec = micro_seconds / 1000000;
	tm.tv_usec = micro_seconds % (1000000);
    select(0, NULL, NULL, NULL, &tm);
}
int gettimeofday(struct timeval *tp, void *tzp) {
	time_t clock;
	struct tm tm;
	SYSTEMTIME wtm;
	GetLocalTime(&wtm);
	tm.tm_year = wtm.wYear - 1900;
	tm.tm_mon = wtm.wMonth - 1;
	tm.tm_mday = wtm.wDay;
	tm.tm_hour = wtm.wHour;
	tm.tm_min = wtm.wMinute;
	tm.tm_sec = wtm.wSecond;
	tm.tm_isdst = -1;
	clock = mktime(&tm);
	tp->tv_sec = clock;
	tp->tv_usec = wtm.wMilliseconds * 1000;
	return (0);
}

#endif //WIN32


}  // namespace Util
}  // namespace ZL

