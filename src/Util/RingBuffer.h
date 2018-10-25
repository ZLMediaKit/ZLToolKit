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
#ifndef UTIL_RINGBUFFER_H_
#define UTIL_RINGBUFFER_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <condition_variable>
#include <functional>

using namespace std;

namespace toolkit {

template<typename T>
class RingDelegate {
public:
	typedef std::shared_ptr<RingDelegate> Ptr;
	RingDelegate(){}
	virtual ~RingDelegate(){}
	virtual void onWrite(const T &in,bool isKey = true) = 0;
};

//实现了一个一写多读得环形缓冲的模板类
template<typename T> class RingBuffer: public enable_shared_from_this<
		RingBuffer<T> > {
public:
	typedef std::shared_ptr<RingBuffer> Ptr;

	class RingReader {
	public:
		friend class RingBuffer;
		typedef std::shared_ptr<RingReader> Ptr;
		RingReader(const std::shared_ptr<RingBuffer> &buffer,bool useBuffer) {
			_buffer = buffer;
			_curpos = buffer->_ringKeyPos;
			_useBuffer = useBuffer;
		}
		~RingReader() {
			auto strongBuffer = _buffer.lock();
			if (strongBuffer) {
				strongBuffer->release(this);
			}
		}
		//重新定位读取器至最新的数据位置
		void reset(bool keypos = true) {
			auto strongBuffer = _buffer.lock();
			if (strongBuffer) {
				if(keypos){
					_curpos = strongBuffer->_ringKeyPos; //定位读取器
				}else{
					_curpos = strongBuffer->_ringPos; //定位读取器
				}
			}
		}
		void setReadCB(const function<void(const T &)> &cb) {
			lock_guard<mutex> lck(_mtxCB);
			_readCB = cb;
			reset();
		}
		void setDetachCB(const function<void()> &cb) {
			lock_guard<mutex> lck(_mtxCB);
			if (!cb) {
				_detachCB = []() {};
			} else {
				_detachCB = cb;
			}
		}
        
        const T* read(){
            auto strongBuffer=_buffer.lock();
            if(!strongBuffer){
                return nullptr;
            }
            return read(strongBuffer.get());
        }
        
	private:
		void onRead(const T &data) {
			lock_guard<mutex> lck(_mtxCB);
			if(!_readCB){
				return;
			}

			if(!_useBuffer){
				_readCB(data);
			}else{
				const T *pkt  = nullptr;
				while((pkt = read())){
					_readCB(*pkt);
				}
			}
		}
		void onDetach() const {
			lock_guard<mutex> lck(_mtxCB);
			_detachCB();
		}
		//读环形缓冲
		const T *read(RingBuffer *ring) {
			if (_curpos == ring->_ringPos) {
				return nullptr;
			}
			const T *data = &(ring->_dataRing[_curpos]); //返回包
			_curpos = ring->next(_curpos); //更新位置
			return data;
		}

	private:
		function<void(const T &)> _readCB ;
		function<void(void)> _detachCB = []() {};
		weak_ptr<RingBuffer> _buffer;
		mutable mutex _mtxCB;
		int _curpos;
		bool _useBuffer;
	};

	friend class RingReader;
	RingBuffer(int size = 0) {
        if(size <= 0){
            size = 32;
            _canReSize = true;
        }
		_ringSize = size;
		_dataRing = new T[_ringSize];
		_ringPos = 0;
		_ringKeyPos = 0;
	}
	~RingBuffer() {
		decltype(_readerMap) mapCopy;
		{
			lock_guard<mutex> lck(_mtx_reader);
			mapCopy.swap(_readerMap);
		}
		for (auto &pr : mapCopy) {
			auto reader = pr.second.lock();
			if(reader){
				reader->onDetach();
			}
		}
		delete[] _dataRing;
	}
#if defined(ENABLE_RING_USEBUF)
	std::shared_ptr<RingReader> attach(bool useBuffer = true) {
#else //ENABLE_RING_USEBUF
	std::shared_ptr<RingReader> attach(bool useBuffer = false) {
#endif //ENABLE_RING_USEBUF

		std::shared_ptr<RingReader> ptr = std::make_shared<RingReader>(this->shared_from_this(),useBuffer);
		std::weak_ptr<RingReader> weakPtr = ptr;
		lock_guard<mutex> lck(_mtx_reader);
		_readerMap.emplace(ptr.get(),weakPtr);
		return ptr;
	}
	//写环形缓冲，非线程安全的
	void write(const T &in,bool isKey = true) {
		{
			lock_guard<mutex> lck(_mtx_delegate);
			if (_delegate) {
				_delegate->onWrite(in, isKey);
			}
		}
		computeBestSize(isKey);
        _dataRing[_ringPos] = in;
        if (isKey) {
            _ringKeyPos = _ringPos; //设置读取器可以定位的点
        }
        _ringPos = next(_ringPos);

        lock_guard<mutex> lck(_mtx_reader);
		for (auto it = _readerMap.begin() ; it != _readerMap.end() ;) {
			auto reader = it->second.lock();
			if(reader){
				reader->onRead(in);
				++it;
			}else{
				it = _readerMap.erase(it);
			}
		}
	}
	int readerCount(){
		lock_guard<mutex> lck(_mtx_reader);
		return _readerMap.size();
	}
	void setDelegate(const typename RingDelegate<T>::Ptr &delegate){
		lock_guard<mutex> lck(_mtx_delegate);
		_delegate = delegate;
	}
private:
	T *_dataRing;
	int _ringPos;
	int _ringKeyPos;
	int _ringSize;
	//计算最佳环形缓存大小的参数
	int _besetSize = 0;
	int _totalCnt = 0;
	int _lastKeyCnt = 0;
    bool _canReSize = false;
	typename RingDelegate<T>::Ptr _delegate;
	mutex _mtx_delegate;
	mutex _mtx_reader;
	unordered_map<void *,std::weak_ptr<RingReader> > _readerMap;

private:
	inline int next(int pos) {
		//读取器下一位置
		if (pos > _ringSize - 2) {
			return 0;
		} else {
			return pos + 1;
		}
	}

	void release(RingReader *reader) {
		if(_mtx_reader.try_lock()){
			_readerMap.erase(reader);
			_mtx_reader.unlock();
		}
	}
	void computeBestSize(bool isKey){
        if(!_canReSize || _besetSize){
			return;
		}
		_totalCnt++;
		if(!isKey){
			return;
		}
		//关键帧
		if(_lastKeyCnt){
			//计算两个I帧之间的包个数
			_besetSize = _totalCnt - _lastKeyCnt;
			if(_besetSize < 32){
				_besetSize = 32;
			}
			if(_besetSize > 512){
				_besetSize = 512;
			}
			reSize();
			return;
		}
		_lastKeyCnt = _totalCnt;
	}
	void reSize(){
		_ringSize = _besetSize;
		delete [] _dataRing;
		_dataRing = new T[_ringSize];
		_ringPos = 0;
		_ringKeyPos = 0;

		lock_guard<mutex> lck(_mtx_reader);
		for (auto &pr : _readerMap) {
			auto reader = pr.second.lock();
			if(reader){
				reader->reset(true);
			}
		}

	}
};

} /* namespace toolkit */

#endif /* UTIL_RINGBUFFER_H_ */
