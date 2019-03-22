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
#include "Poller/EventPoller.h"

using namespace std;

//自适应环形缓存大小的取值范围
#define RING_MIN_SIZE 32
#define RING_MAX_SIZE 512
#define LOCK_GUARD(mtx) lock_guard<decltype(mtx)> lck(mtx)

namespace toolkit {

template<typename T>
class RingDelegate {
public:
	typedef std::shared_ptr<RingDelegate> Ptr;
	RingDelegate(){}
	virtual ~RingDelegate(){}
	virtual void onWrite(const T &in,bool isKey = true) = 0;
};

template<typename T>
class _RingStorage;

template<typename T>
class _RingReaderDispatcher;

/**
 * 环形缓存读取器
 * 该对象的事件触发都会在绑定的poller线程中执行
 * 所以把锁去掉了
 * 对该对象的一切操作都应该在poller线程中执行
 * @tparam T
 */
template<typename T>
class _RingReader {
public:
    typedef std::shared_ptr<_RingReader> Ptr;
    friend class _RingReaderDispatcher<T>;

    _RingReader(const std::shared_ptr<_RingStorage<T> > &storage,bool useBuffer) {
        _storage = storage;
        _curpos = storage->getPos(true);
        _useBuffer = useBuffer;
    }

    ~_RingReader() {}

    void setReadCB(const function<void(const T &)> &cb) {
        if (!cb) {
            _readCB = [](const T &) {};
        } else {
            _readCB = cb;
        }
        resetPos();
    }

    void setDetachCB(const function<void()> &cb) {
        if (!cb) {
            _detachCB = []() {};
        } else {
            _detachCB = cb;
        }
    }

    //重新定位至最新的数据位置
    void resetPos(bool keypos = true) {
        _curpos = _storage->getPos(keypos);
    }
private:
    void onRead(const T &data) {
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
        _detachCB();
    }

    const T* read(){
        if (_curpos == _storage->getPos(false)) {
            return nullptr;
        }
        const T *data = &((*_storage)[_curpos]); //返回包
        _curpos = _storage->next(_curpos); //更新位置
        return data;
    }
private:
    function<void(const T &)> _readCB = [](const T &) {};
    function<void(void)> _detachCB = []() {};
    shared_ptr<_RingStorage<T> > _storage;
    int _curpos;
    bool _useBuffer;
};

template<typename T>
class _RingStorage{
public:
    typedef std::shared_ptr<_RingStorage> Ptr;

    _RingStorage(int size = 0){
        if(size <= 0){
            size = RING_MIN_SIZE;
            _canReSize = true;
        }
        _ringSize = size;
        _dataRing = new T[_ringSize];
        _ringPos = 0;
        _ringKeyPos = 0;
    }

    ~_RingStorage(){
        delete[] _dataRing;
    }

    void setDelegate(const typename RingDelegate<T>::Ptr &delegate){
        LOCK_GUARD(_mtx_delegate);
        _delegate = delegate;
    }

    /**
     * 写入环形缓存数据
     * @param in 数据
     * @param isKey 是否为关键帧
     * @return 是否触发重置环形缓存大小
     */
    bool write(const T &in,bool isKey = true) {
        {
            LOCK_GUARD(_mtx_delegate);
            if (_delegate) {
                _delegate->onWrite(in, isKey);
            }
        }
        auto flag = computeGopSize(isKey);
        _dataRing[_ringPos] = in;
        if (isKey) {
            _ringKeyPos = _ringPos; //设置读取器可以定位的点
        }
        _ringPos = next(_ringPos);
        return flag;
    }

    int getPos(bool keypos){
        return  keypos ?  _ringKeyPos : _ringPos;
    }

    int next(int pos) {
        //读取器下一位置
        if (pos > _ringSize - 2) {
            return 0;
        } else {
            return pos + 1;
        }
    }

    T& operator[](int pos){
        return _dataRing[pos];
    }
private:
    void reSize(){
        _ringSize = _besetSize;
        delete [] _dataRing;
        _dataRing = new T[_ringSize];
        _ringPos = 0;
        _ringKeyPos = 0;
    }

    bool computeGopSize(bool isKey){
        if(!_canReSize || _besetSize){
            return false;
        }
        _totalCnt++;
        if(!isKey){
            return false;
        }
        //关键帧
        if(!_lastKeyCnt){
            //第一次获取关键帧
            _lastKeyCnt = _totalCnt;
            return false;
        }

        //第二次获取关键帧，计算两个I帧之间的包个数
        _besetSize = _totalCnt - _lastKeyCnt;
        if(_besetSize < RING_MIN_SIZE){
            _besetSize = RING_MIN_SIZE;
        }
        if(_besetSize > RING_MAX_SIZE){
            _besetSize = RING_MAX_SIZE;
        }
        reSize();
        return true;
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
};

template<typename T>
class RingBuffer;

/**
 * 环形缓存事件派发器，只能一个poller线程操作它
 * @tparam T
 */
template<typename T>
class _RingReaderDispatcher: public enable_shared_from_this<_RingReaderDispatcher<T> > {
public:
	typedef std::shared_ptr<_RingReaderDispatcher> Ptr;
    typedef _RingReader<T> RingReader;
    typedef _RingStorage<T> RingStorage;
    friend class RingBuffer<T>;

	~_RingReaderDispatcher() {
        decltype(_readerMap) mapCopy;
        mapCopy.swap(_readerMap);
        for (auto &pr : mapCopy) {
            auto reader = pr.second.lock();
            if(reader){
                reader->onDetach();
            }
        }
	}

private:
    _RingReaderDispatcher(const typename RingStorage::Ptr &storage,const function<void() > &onClear ) {
        _storage = storage;
        _readerSize = 0;
        _onClear = onClear;
    }

    void emitRead(const T &in){
        for (auto it = _readerMap.begin() ; it != _readerMap.end() ;) {
            auto reader = it->second.lock();
            if(!reader){
                it = _readerMap.erase(it);
                --_readerSize;
                onSizeChanged();
                continue;
            }
            reader->onRead(in);
            ++it;
        }
	}

    void resetPos(bool keypos = true)  {
        for (auto it = _readerMap.begin() ; it != _readerMap.end() ;) {
            auto reader = it->second.lock();
            if(!reader){
                it = _readerMap.erase(it);
                --_readerSize;
                onSizeChanged();
                continue;
            }
            reader->resetPos(keypos);
            ++it;
        }
    }

    std::shared_ptr<RingReader> attach(const EventPoller::Ptr &poller,bool useBuffer) {
	    if(!poller->isCurrentThread()){
	        throw std::runtime_error("必须在绑定的poller线程中执行attach操作");
	    }

        weak_ptr<_RingReaderDispatcher> weakSelf = this->shared_from_this();
	    auto onDealloc = [weakSelf,poller](RingReader *ptr){
            poller->async([weakSelf,ptr](){
                auto strongSelf = weakSelf.lock();
                if(strongSelf && strongSelf->_readerMap.erase(ptr)){
                    --strongSelf->_readerSize;
                    strongSelf->onSizeChanged();
                }
                delete ptr;
            });
	    };

        std::shared_ptr<RingReader> reader(new RingReader(_storage,useBuffer),onDealloc);
        _readerMap[reader.get()] = std::move(reader);
        ++ _readerSize;
        onSizeChanged();
        return reader;
    }

    int readerCount(){
        return _readerSize;
	}

    void onSizeChanged(){
        if(_readerSize == 0){
            _onClear();
        }
	}
private:
    function<void() > _onClear;
    atomic_int _readerSize;
    typename RingStorage::Ptr _storage;
    unordered_map<void *,std::weak_ptr<RingReader> > _readerMap;
};

template<typename T>
class RingBuffer :  public enable_shared_from_this<RingBuffer<T> > {
public:
    typedef std::shared_ptr<RingBuffer> Ptr;
    typedef _RingReader<T> RingReader;
    typedef _RingStorage<T> RingStorage;
    typedef _RingReaderDispatcher<T> RingReaderDispatcher;

    RingBuffer(int size = 0) {
        _storage = std::make_shared<RingStorage>(size);
    }

    ~RingBuffer() {}

    void write(const T &in, bool isKey = true) {
        if (_storage->write(in, isKey)) {
            resetPos();
        }
        emitRead(in);
    }

    void setDelegate(const typename RingDelegate<T>::Ptr &delegate) {
        _storage->setDelegate(delegate);
    }

    std::shared_ptr<RingReader> attach(const EventPoller::Ptr &poller, bool useBuffer = true) {
        typename RingReaderDispatcher::Ptr dispatcher;
        {
            LOCK_GUARD(_mtx_map);
            auto &ref = _dispatcherMap[poller];
            if (!ref) {
                weak_ptr<RingBuffer> weakSelf = this->shared_from_this();
                auto onClear = [weakSelf,poller](){
                    auto strongSelf = weakSelf.lock();
                    if(!strongSelf){
                        return;
                    }
                    strongSelf->erase(poller);
                };

                auto onDealloc = [poller](RingReaderDispatcher *ptr){
                    poller->async([ptr](){
                        delete ptr;
                    });
                };
                ref.reset(new RingReaderDispatcher(_storage,std::move(onClear)),std::move(onDealloc));
            }
            dispatcher = ref;
        }

        return dispatcher->attach(poller,useBuffer);
    }

    int readerCount() {
        LOCK_GUARD(_mtx_map);
        int total = 0;
        for (auto &pr : _dispatcherMap){
            total += pr.second->readerCount();
        }
        return total;
    }
private:
    void resetPos(bool keypos = true ) {
        LOCK_GUARD(_mtx_map);
        for (auto &pr : _dispatcherMap) {
            auto second = pr.second;
            pr.first->async([second,keypos](){
                second->resetPos(keypos);
            },false);
        }
    }
    void emitRead(const T &in){
        LOCK_GUARD(_mtx_map);
        for (auto &pr : _dispatcherMap) {
            auto second = pr.second;
            pr.first->async([second,in](){
                second->emitRead(in);
            },false);
        }
    }
    void erase(const EventPoller::Ptr &poller){
        LOCK_GUARD(_mtx_map);
        _dispatcherMap.erase(poller);
    }
private:
    struct HashOfPtr {
        std::size_t operator()(const EventPoller::Ptr &key) const
        {
            return (uint64_t)key.get();
        }
    };
private:
    typename RingStorage::Ptr _storage;
    mutex _mtx_map;
    unordered_map<EventPoller::Ptr,typename RingReaderDispatcher::Ptr,HashOfPtr > _dispatcherMap;
};

}; /* namespace toolkit */

#endif /* UTIL_RINGBUFFER_H_ */
