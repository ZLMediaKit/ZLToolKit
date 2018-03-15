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

#ifndef UTIL_TIMETICKER_H_
#define UTIL_TIMETICKER_H_

#include "logger.h"
#include "Util/util.h"

namespace ZL {
namespace Util {

class Ticker {
public:
	Ticker(int64_t _minMs = 0, const char *_where = "",
			LogInfoMaker && _stream = LogInfoMaker(LWarn, __FILE__, "", __LINE__),bool printLog=false) :
			stream(_stream) {
		if(!printLog){
			stream.clear();
		}
		begin = getNowTime();
		created = begin;
		minMs = _minMs;
		where = _where;
	}
	virtual ~Ticker() {
		int64_t tm = getNowTime() - begin;
		if (tm > minMs) {
			stream << where << " take time:" << tm << endl;
		} else {
			stream.clear();
		}
	}
	uint64_t elapsedTime() {
		stream.clear();
		return getNowTime() - begin;
	}
	uint64_t createdTime() {
		stream.clear();
		return getNowTime() - created;
	}
	void resetTime() {
		stream.clear();
		begin = getNowTime();
	}

private:
	inline static uint64_t getNowTime() {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		return tv.tv_sec * 1000 + tv.tv_usec / 1000;
	}
	uint64_t begin;
	uint64_t created;
	LogInfoMaker stream;
	const char *where;
	int64_t minMs;

};
class SmoothTicker {
public:
	SmoothTicker(uint64_t _resetMs = 10000) {
		resetMs = _resetMs;
		ticker.resetTime();
	}
	virtual ~SmoothTicker() {
	}
	uint64_t elapsedTime() {
		auto nowTime = ticker.elapsedTime();
		if (firstTime == 0) {
			firstTime = nowTime;
			lastTime = nowTime;
			pktCount = 0;
			return nowTime;
		}
		uint64_t elapseTime = (nowTime - firstTime);
		uint64_t retTime = lastTime + elapseTime / ++pktCount;
		lastTime = retTime;
		if (elapseTime > 10000) {
			firstTime = 0;
		}
		return retTime;
	}
    void resetTime(){
        firstTime = 0;
        pktCount = 0;
        lastTime = 0;
        ticker.resetTime();
    }
private:
	uint64_t firstTime = 0;
	uint64_t pktCount = 0;
	uint64_t lastTime = 0;
	uint64_t resetMs;
	Ticker ticker;
};

#if defined(_DEBUG)
	#define TimeTicker() Ticker __ticker(5,"",WarnL,true)
	#define TimeTicker1(tm) Ticker __ticker1(tm,"",WarnL,true)
	#define TimeTicker2(tm,where) Ticker __ticker2(tm,where,WarnL,true)
	#define TimeTicker3(tm,where,log) Ticker __ticker3(tm,where,log,true)
#else
	#define TimeTicker()
	#define TimeTicker1(tm)
	#define TimeTicker2(tm,where)
	#define TimeTicker3(tm,where,log)
#endif
} /* namespace Util */
} /* namespace ZL */

#endif /* UTIL_TIMETICKER_H_ */
