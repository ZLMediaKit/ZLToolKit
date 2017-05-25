/*
 * util.h
 *
 *  Created on: 2016年8月4日
 *      Author: xzl
 */
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <algorithm>
#include "util.h"

using namespace std;

namespace ZL {
namespace Util {
string makeRandStr(int sz, bool printable) {
	char tmp[sz + 1];
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
	return tmp;
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
	char buffer[256];
#ifdef __WIN32__
	int n = -1;
#else
	int n = readlink("/proc/self/exe", buffer, sizeof(buffer));
#endif //__WIN32__
	if (n <= 0) {
		filePath = _exePath;
	} else {
		filePath.assign(buffer, n);
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


#ifdef __WIN32__
int strcasecmp(const char *strA,const char *strB){
	string str1 = strToLower(strA);
	string str2 = strToLower(strB);
	return str1.compare(str2);
}
#endif //WIN32

}  // namespace Util
}  // namespace ZL

