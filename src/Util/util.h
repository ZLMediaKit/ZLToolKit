/*
 * util.h
 *
 *  Created on: 2016年8月4日
 *      Author: xzl
 */

#ifndef UTIL_UTIL_H_
#define UTIL_UTIL_H_
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

string makeRandStr(int sz);
}  // namespace Util
}  // namespace ZL

#endif /* UTIL_UTIL_H_ */
