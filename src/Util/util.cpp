/*
 * util.h
 *
 *  Created on: 2016年8月4日
 *      Author: xzl
 */

#include "util.h"

namespace ZL {
namespace Util {
string makeRandStr(int sz) {
	char tmp[sz + 1];
	static const char CCH[] =
			"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int i;
	for (i = 0; i < sz; i++) {
		srand((unsigned) time(NULL) + i);
		int x = rand() % (sizeof(CCH) - 1);
		tmp[i] = CCH[x];
	}
	tmp[i] = 0;
	return tmp;
}

}  // namespace Util
}  // namespace ZL

