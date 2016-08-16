/*
 * TimeTicker.h
 *
 *  Created on: 2015年12月30日
 *      Author: root
 */

#ifndef UTIL_TIMETICKER_H_
#define UTIL_TIMETICKER_H_
#include <sys/time.h>
#include "logger.h"

namespace ZL {
namespace Util {

class _TimeTicker {
public:
	_TimeTicker(int64_t _minMs = 0, const char *_where = "",
			LogInfoMaker && _stream = WarnL) :
			stream(_stream) {
		begin = getNowTime();
		minMs = _minMs;
		where = _where;
	}
	virtual ~_TimeTicker() {
		int64_t tm = getNowTime() - begin;
		if (tm > minMs) {
			stream << where << "执行时间:" << tm << endl;
		}else{
			stream.clear();
		}
	}
private:
	inline static uint64_t getNowTime() {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		return tv.tv_sec * 1000 + tv.tv_usec / 1000;
	}
	uint64_t begin;
	LogInfoMaker stream;
	const char *where;
	int64_t minMs;

};
#define TimeTicker() _TimeTicker __ticker(5,"",WarnL)
#define TimeTicker1(tm) _TimeTicker __ticker1(tm,"",WarnL)
#define TimeTicker2(tm,where) _TimeTicker __ticker2(tm,where,WarnL)
#define TimeTicker3(tm,where,log) _TimeTicker __ticker3(tm,where,log)
} /* namespace Util */
} /* namespace ZL */

#endif /* UTIL_TIMETICKER_H_ */
