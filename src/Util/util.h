/*
 * util.h
 *
 *  Created on: 2016年8月4日
 *      Author: xzl
 */

#ifndef UTIL_UTIL_H_
#define UTIL_UTIL_H_

#include <string.h>
#include <string>
#include <sstream>

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
void setExePath(const string &path);


int compareNoCase(const char *strA,const char *strB);

#ifndef strcasecmp
#define strcasecmp compareNoCase
#endif //strcasecmp

}  // namespace Util
}  // namespace ZL

#endif /* UTIL_UTIL_H_ */
