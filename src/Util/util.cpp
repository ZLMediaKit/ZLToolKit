/*
 * util.h
 *
 *  Created on: 2016年8月4日
 *      Author: xzl
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
	string filePath;
	char buffer[1024] = {0};
#if defined(_WIN32)
	wchar_t szExePath[MAX_PATH] = { 0 };
	int wcSize = GetModuleFileNameW(NULL, szExePath, sizeof(szExePath));
	size_t n = 0;
	wcstombs_s(&n, buffer, wcSize + 1, szExePath, _TRUNCATE);
#else
	int n = readlink("/proc/self/exe", buffer, sizeof(buffer));
#endif //WIN32
	if (n <= 0) {
		filePath = _exePath;
	} else {
		filePath = buffer;
	}
	for (auto &ch : filePath) {
		if (ch == '\\') {
			ch = '/';
		}
	}
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
int strcasecmp(const char *strA,const char *strB){
	string str1 = strToLower(strA);
	string str2 = strToLower(strB);
	return str1.compare(str2);
}
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

