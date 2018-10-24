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

namespace toolkit {

class Ticker {
public:
	Ticker(int64_t minMs = 0,
		   const char *where = "",
		   LogInfoMaker && stream = LogInfoMaker(LWarn, __FILE__, "", __LINE__),
		   bool printLog=false):_stream(stream) {
		if(!printLog){
			_stream.clear();
		}
		_begin = getNowTime();
		_created = _begin;
		_minMs = minMs;
		_where = where;
	}
	~Ticker() {
		int64_t tm = getNowTime() - _begin;
		if (tm > _minMs) {
			_stream << _where << " take time:" << tm << endl;
		} else {
			_stream.clear();
		}
	}
	uint64_t elapsedTime() {
		_stream.clear();
		return getNowTime() - _begin;
	}
	uint64_t createdTime() {
		_stream.clear();
		return getNowTime() - _created;
	}
	void resetTime() {
		_stream.clear();
		_begin = getNowTime();
	}

private:
	inline static uint64_t getNowTime() {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		return tv.tv_sec * 1000 + tv.tv_usec / 1000;
	}
private:
	uint64_t _begin;
	uint64_t _created;
	LogInfoMaker _stream;
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
			_lastTime = nowTime;
			_pktCount = 0;
			return nowTime;
		}
		uint64_t elapseTime = (nowTime - _firstTime);
		uint64_t retTime = _lastTime + elapseTime / ++_pktCount;
		_lastTime = retTime;
		if (elapseTime > 10000) {
			_firstTime = 0;
		}
		return retTime;
	}
    void resetTime(){
		_firstTime = 0;
		_pktCount = 0;
		_lastTime = 0;
		_ticker.resetTime();
    }
private:
	uint64_t _firstTime = 0;
	uint64_t _pktCount = 0;
	uint64_t _lastTime = 0;
	uint64_t _resetMs;
	Ticker _ticker;
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

} /* namespace toolkit */

#endif /* UTIL_TIMETICKER_H_ */
