/*
 * RingBuffer.h
 *
 *  Created on: 2016年8月10日
 *      Author: xzl
 */

#ifndef UTIL_RINGBUFFER_HPP_
#define UTIL_RINGBUFFER_HPP_
#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <unordered_set>
using namespace std;

namespace ZL {
namespace Util {

//实现了一个一写多读得环形缓冲的模板类
template<typename T> class RingBuffer: public enable_shared_from_this<
		RingBuffer<T> > {
public:
	typedef shared_ptr<RingBuffer> Ptr;

	class RingReader {
	public:
		friend class RingBuffer;
		typedef shared_ptr<RingReader> Ptr;
		RingReader(const shared_ptr<RingBuffer> &_buffer) {
			buffer = _buffer;
			curpos = _buffer->ringKeyPos.load();
		}
		virtual ~RingReader() {
			auto strongBuffer = buffer.lock();
			if (strongBuffer) {
				strongBuffer->release(this);
			}
		}
		//读环形缓冲
		bool read(T &data, bool block = true) {
			auto strongBuffer = buffer.lock();
			if (!strongBuffer) {
				return false;
			}
			while (curpos == strongBuffer->ringPos) {
				if (!block) {
					return false;
				}
				strongBuffer->wait();
			}
			data = strongBuffer->dataRing[curpos]; //返回包
			curpos = strongBuffer->next(curpos); //更新位置
			return true;
		}
		//重新定位读取器至最新的数据位置
		void reset() {
			auto strongBuffer = buffer.lock();
			if (strongBuffer) {
				curpos = strongBuffer->ringPos.load(); //定位读取器
			}
		}
		void setReadCB(const function<void(const T &)> &cb) {
			if (!cb) {
				readCB = [](const T &) {};
			} else {
				readCB = cb;
			}
		}
		void setDetachCB(const function<void()> &cb) {
			if (!cb) {
				detachCB = []() {};
			} else {
				detachCB = cb;
			}
		}
	private:
		void onRead(const T &data) {
			readCB(data);
		}
		void onDetach() {
			detachCB();
		}
		function<void(const T &)> readCB = [](const T &) {};
		function<void(void)> detachCB = []() {};
		weak_ptr<RingBuffer> buffer;
		int curpos;
	};

	friend class RingReader;
	RingBuffer(int _ringSize = 32) {
		ringSize = _ringSize;
		dataRing = new T[ringSize];
		ringPos = 0;
		ringKeyPos = 0;
	}
	virtual ~RingBuffer() {
		{
			unique_lock<mutex> lock(mtx_cond);
			cond.notify_all();
		}
		lock_guard<mutex> lck(mtx_reader);
		for (auto reader : readerSet) {
			reader->onDetach();
		}
		delete[] dataRing;
	}

	shared_ptr<RingReader> attach() {
		shared_ptr<RingReader> ret(new RingReader(this->shared_from_this()));
		lock_guard<mutex> lck(mtx_reader);
		readerSet.emplace(ret.get());
		return ret;
	}
	//写环形缓冲，非线程安全的
	void write(const T &in, bool isKey = true) {
		{
			lock_guard<mutex> lck(mtx_reader);
			for (auto reader : readerSet) {
				reader->onRead(in);
			}
		}
		dataRing[ringPos] = in;
		if (isKey) {
			ringKeyPos = ringPos.load(); //设置读取器可以定位的点
		}
		ringPos = next(ringPos);
		unique_lock<mutex> lock(mtx_cond);
		cond.notify_all();
	}

private:
	T *dataRing;
	atomic_long ringPos;
	atomic_long ringKeyPos;
	int ringSize;

	mutex mtx_reader;
	unordered_set<RingReader *> readerSet;

	mutex mtx_cond;
	condition_variable cond;

	inline long next(long pos) {
		//读取器下一位置
		if (pos > ringSize - 2) {
			return 0;
		} else {
			return pos + 1;
		}
	}

	void release(RingReader *reader) {
		lock_guard<mutex> lck(mtx_reader);
		readerSet.erase(reader);
	}
	void wait() {
		unique_lock<mutex> lock(mtx_cond);
		cond.wait(lock);
	}

};

} /* namespace Util */
} /* namespace ZL */

#endif /* UTIL_RINGBUFFER_HPP_ */
