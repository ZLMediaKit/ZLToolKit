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

#ifndef UTIL_TIMETICKER_H_
#define UTIL_TIMETICKER_H_

#include <assert.h>
#include "logger.h"
#include "Util/util.h"

namespace toolkit {

class Ticker {
public:
	Ticker(int64_t minMs = 0,
		   LogContextCapturer && ctx = LogContextCapturer(Logger::Instance(),LWarn, __FILE__, "", __LINE__),
		   bool printLog = false):_ctx(ctx) {
		if(!printLog){
			_ctx.clear();
		}
        _created = _begin = getCurrentMillisecond();
		_minMs = minMs;
	}
	~Ticker() {
		int64_t tm = createdTime();
		if (tm > _minMs) {
			_ctx << "take time:" << tm << "ms" << endl;
		} else {
			_ctx.clear();
		}
	}
	uint64_t elapsedTime() {
		return getCurrentMillisecond() - _begin;
	}
	uint64_t createdTime() {
		return getCurrentMillisecond() - _created;
	}
	void resetTime() {
		_begin = getCurrentMillisecond();
	}
private:
	uint64_t _begin;
	uint64_t _created;
	LogContextCapturer _ctx;
	const char *_where;
	int64_t _minMs;

};
class SmoothTicker {
public:
	SmoothTicker(uint64_t resetMs = 10000) {
		_resetMs = resetMs;
		_ticker.resetTime();
	}
	~SmoothTicker() {
	}
	uint64_t elapsedTime() {
		auto nowTime = _ticker.elapsedTime();
		if (_firstTime == 0) {
			_firstTime = nowTime;
			_pktCount = 0;
			_timeInc = 0;
			return nowTime;
		}

		double elapseTime = (nowTime - _firstTime);
		_timeInc += elapseTime / ++_pktCount;
		uint64_t retTime = _firstTime + _timeInc;
		if (elapseTime > _resetMs) {
			_firstTime = 0;
		}
		return retTime;
	}
    void resetTime(){
		_firstTime = 0;
		_pktCount = 0;
		_ticker.resetTime();
    }
private:
	double _timeInc = 0;
	uint64_t _firstTime = 0;
	uint64_t _pktCount = 0;
	uint64_t _resetMs;
	Ticker _ticker;
};

#if !defined(NDEBUG)
	#define TimeTicker() Ticker __ticker(5,WarnL,true)
	#define TimeTicker1(tm) Ticker __ticker1(tm,WarnL,true)
	#define TimeTicker2(tm,log) Ticker __ticker2(tm,log,true)
#else
	#define TimeTicker()
	#define TimeTicker1(tm)
	#define TimeTicker2(tm,log)
#endif

} /* namespace toolkit */

#endif /* UTIL_TIMETICKER_H_ */
