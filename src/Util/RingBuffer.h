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
class _RingStorageInternal;

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
        _useBuffer = useBuffer;
    }

    ~_RingReader() {}

    void setReadCB(const function<void(const T &)> &cb) {
        if (!cb) {
            _readCB = [](const T &) {};
        } else {
            _readCB = cb;
            resetPos(true);
        }
    }

    void setDetachCB(const function<void()> &cb) {
        if (!cb) {
            _detachCB = []() {};
        } else {
            _detachCB = cb;
        }
    }

    //重新定位至最新的数据位置
    void resetPos(bool key = true) {
        _storageInternal = _storage->getStorageInternal();
        _curpos = _storageInternal->getPos(key);
        if(key && _useBuffer){
            flushGop();
        }
    }
private:
    void onRead(const T &data) {
        _readCB(data);
    }

    void flushGop(){
        const T *pkt  = nullptr;
        auto targetPos = _storageInternal->getPos(false);
        while((pkt = read(targetPos))){
            _readCB(*pkt);
        }
    }

    void onDetach() const {
        _detachCB();
    }

    const T* read(int targetPos){
        while (_curpos != targetPos){
            const T *data = (*_storageInternal)[_curpos]; //返回包
            _curpos = _storageInternal->next(_curpos); //更新位置
            if(!data){
                continue;
            }
            return data;
        }
        return nullptr;
    }
private:
    function<void(const T &)> _readCB = [](const T &) {};
    function<void(void)> _detachCB = []() {};

    shared_ptr<_RingStorageInternal<T> > _storageInternal;
    shared_ptr<_RingStorage<T> > _storage;
    int _curpos;
    bool _useBuffer;
};

template<typename T>
class _RingStorageInternal{
public:
    typedef std::shared_ptr<_RingStorageInternal> Ptr;
    _RingStorageInternal(int size){
        _dataRing.resize(size);
        _ringSize = size;
        _ringKeyPos = size;
        _ringPos = 0;
    }

    ~_RingStorageInternal(){}


    /**
     * 写入环形缓存数据
     * @param in 数据
     * @param isKey 是否为关键帧
     * @return 是否触发重置环形缓存大小
     */
    inline void write(const T &in,bool key = true) {
        _dataRing[_ringPos].second = in;
        _dataRing[_ringPos].first = true;
        if (key) {
            _ringKeyPos = _ringPos; //设置读取器可以定位的点
        }
        _ringPos = next(_ringPos);
    }

    inline int getPos(bool key){
        if(!key){
            //不是定位关键帧，那么返回当前位置即可
            return _ringPos;
        }
        //定位关键帧
        if(_ringKeyPos != _ringSize){
            //环线缓存中存在关键帧，返回之
            return _ringKeyPos;
        }

        //如果环形缓存中没有关键帧，
        //那么我们返回当前位置后面的偏移1的位置,
        //这样能把环形缓存中的所有缓存一次性输出
        int ret = _ringPos + 1;
        if(ret < _ringSize){
            return ret;
        }
        return ret - _ringSize;
    }

    inline int next(int pos) {
        //读取器下一位置
        if (pos > _ringSize - 2) {
            return 0;
        } else {
            return pos + 1;
        }
    }

    inline T* operator[](int pos){
        auto &ref = _dataRing[pos];
        if(!ref.first){
            return nullptr;
        }
        return &ref.second;
    }
    Ptr clone() const{
        Ptr ret(new _RingStorageInternal());
        ret->_dataRing = _dataRing;
        ret->_ringPos = _ringPos;
        ret->_ringKeyPos = _ringKeyPos;
        ret->_ringSize = _ringSize;
        return ret;
    }

    int size() const{
        return _ringSize;
    }
private:
    class DataPair : public std::pair<bool,T>{
    public:
        DataPair() : std::pair<bool,T>(false,T()) {}
        ~DataPair() = default;
    };
    _RingStorageInternal() = default;
private:
    vector<DataPair> _dataRing;
    int _ringPos;
    int _ringKeyPos;
    int _ringSize;
};

template<typename T>
class _RingStorage{
public:
    typedef std::shared_ptr<_RingStorage> Ptr;

    _RingStorage(int size){
        if(size <= 0){
            size = RING_MIN_SIZE;
            _canReSize = true;
        }
        _storageInternal = std::make_shared< _RingStorageInternal<T> >(size);
    }

    ~_RingStorage(){}


    /**
     * 写入环形缓存数据
     * @param in 数据
     * @param isKey 是否为关键帧
     * @return 是否触发重置环形缓存大小
     */
    inline bool write(const T &in,bool isKey = true) {
        auto flag = computeGopSize(isKey);
        _storageInternal->write(in,isKey);
        return flag;
    }

    typename _RingStorageInternal<T>::Ptr getStorageInternal(){
        return _storageInternal;
    }

    inline int getPos(bool key){
        return _storageInternal->getPos(key);
    }

    Ptr clone() const{
        Ptr ret(new _RingStorage());
        ret->_besetSize = _besetSize;
        ret->_totalCnt = _totalCnt;
        ret->_lastKeyCnt = _lastKeyCnt;
        ret->_canReSize = _canReSize;
        ret->_storageInternal = _storageInternal->clone();
        return ret;
    }
private:
    _RingStorage() = default;

    inline bool computeGopSize(bool isKey){
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
        //缓存最多2个GOP，确保缓存中最少有一个GOP
        _besetSize = (_totalCnt - _lastKeyCnt) * 2;
        if(_besetSize < RING_MIN_SIZE){
            _besetSize = RING_MIN_SIZE;
        }
        if(_besetSize > RING_MAX_SIZE){
            _besetSize = RING_MAX_SIZE;
        }
        _storageInternal = std::make_shared< _RingStorageInternal<T> >(_besetSize);
        return true;
    }
private:
    typename _RingStorageInternal<T>::Ptr _storageInternal;
    //计算最佳环形缓存大小的参数
    int _besetSize = 0;
    int _totalCnt = 0;
    int _lastKeyCnt = 0;
    bool _canReSize = false;
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
    _RingReaderDispatcher(const typename RingStorage::Ptr &storage,const function<void(int,bool) > &onSizeChanged ) {
        _storage = storage;
        _readerSize = 0;
        _onSizeChanged = onSizeChanged;
    }

    void write(const T &in, bool isKey = true) {
        if (_storage->write(in, isKey)) {
            resetPos();
        }else{
            emitRead(in);
        }
    }

    void emitRead(const T &in){
        for (auto it = _readerMap.begin() ; it != _readerMap.end() ;) {
            auto reader = it->second.lock();
            if(!reader){
                it = _readerMap.erase(it);
                --_readerSize;
                onSizeChanged(false);
                continue;
            }
            reader->onRead(in);
            ++it;
        }
	}

    void resetPos(bool key = true)  {
        for (auto it = _readerMap.begin() ; it != _readerMap.end() ;) {
            auto reader = it->second.lock();
            if(!reader){
                it = _readerMap.erase(it);
                --_readerSize;
                onSizeChanged(false);
                continue;
            }
            reader->resetPos(key);
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
                    strongSelf->onSizeChanged(false);
                }
                delete ptr;
            });
	    };

        std::shared_ptr<RingReader> reader(new RingReader(_storage,useBuffer),onDealloc);
        _readerMap[reader.get()] = std::move(reader);
        ++ _readerSize;
        onSizeChanged(true);
        return reader;
    }

    int readerCount(){
        return _readerSize;
	}

    void onSizeChanged(bool add_flag){
        _onSizeChanged(_readerSize,add_flag);
	}
private:
    function<void(int,bool) > _onSizeChanged;
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
    typedef function<void(const EventPoller::Ptr &poller,int size,bool add_flag)> onReaderChanged;

    RingBuffer(int size = 0,const onReaderChanged &cb = nullptr) {
        _onReaderChanged = cb;
        _storage = std::make_shared<RingStorage>(size);
    }

    ~RingBuffer() {}

    void write(const T &in, bool isKey = true) {
        if(_delegate){
            _delegate->onWrite(in,isKey);
            return;
        }

        LOCK_GUARD(_mtx_map);
        _storage->write(in, isKey);
        for (auto &pr : _dispatcherMap) {
            auto &second = pr.second;
            pr.first->async([second,in,isKey](){
                second->write(in,isKey);
            },false);
        }
    }

    void setDelegate(const typename RingDelegate<T>::Ptr &delegate) {
        _delegate = delegate;
    }

    std::shared_ptr<RingReader> attach(const EventPoller::Ptr &poller, bool useBuffer = true) {
        typename RingReaderDispatcher::Ptr dispatcher;
        {
            LOCK_GUARD(_mtx_map);
            auto &ref = _dispatcherMap[poller];
            if (!ref) {
                weak_ptr<RingBuffer> weakSelf = this->shared_from_this();
                auto onSizeChanged = [weakSelf,poller](int size,bool add_flag){
                    auto strongSelf = weakSelf.lock();
                    if(!strongSelf){
                        return;
                    }
                    strongSelf->onSizeChanged(poller,size,add_flag);
                };

                auto onDealloc = [poller](RingReaderDispatcher *ptr){
                    poller->async([ptr](){
                        delete ptr;
                    });
                };
                ref.reset(new RingReaderDispatcher(_storage->clone(),std::move(onSizeChanged)),std::move(onDealloc));
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
    void onSizeChanged(const EventPoller::Ptr &poller,int size,bool add_flag){
        if(size == 0){
            LOCK_GUARD(_mtx_map);
            _dispatcherMap.erase(poller);
        }

        if(_onReaderChanged){
            _onReaderChanged(poller,size,add_flag);
        }
    }
private:
    struct HashOfPtr {
        std::size_t operator()(const EventPoller::Ptr &key) const
        {
            return (uint64_t)key.get();
        }
    };
private:
    mutex _mtx_map;
    typename RingStorage::Ptr _storage;
    typename RingDelegate<T>::Ptr _delegate;
    onReaderChanged _onReaderChanged;
    unordered_map<EventPoller::Ptr,typename RingReaderDispatcher::Ptr,HashOfPtr > _dispatcherMap;
};

}; /* namespace toolkit */

#endif /* UTIL_RINGBUFFER_H_ */
