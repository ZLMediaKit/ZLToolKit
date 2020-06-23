/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef UTIL_RINGBUFFER_H_
#define UTIL_RINGBUFFER_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <condition_variable>
#include <functional>
#include <deque>
#include "Poller/EventPoller.h"
using namespace std;

//GOP缓存最大长度下限值
#define RING_MIN_SIZE 32
#define LOCK_GUARD(mtx) lock_guard<decltype(mtx)> lck(mtx)

namespace toolkit {

template<typename T>
class RingDelegate {
public:
    typedef std::shared_ptr<RingDelegate> Ptr;
    RingDelegate() {}
    virtual ~RingDelegate() {}
    virtual void onWrite(const T &in, bool is_key = true) = 0;
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

    _RingReader(const std::shared_ptr<_RingStorage<T> > &storage, bool use_cache) {
        _storage = storage;
        _use_cache = use_cache;
    }

    ~_RingReader() {}

    void setReadCB(const function<void(const T &)> &cb) {
        if (!cb) {
            _read_cb = [](const T &) {};
        } else {
            _read_cb = cb;
            flushGop();
        }
    }

    void setDetachCB(const function<void()> &cb) {
        if (!cb) {
            _detach_cb = []() {};
        } else {
            _detach_cb = cb;
        }
    }

private:
    void onRead(const T &data, bool is_key) {
        _read_cb(data);
    }

    void onDetach() const {
        _detach_cb();
    }

    void flushGop() {
        if (!_use_cache) {
            return;
        }
        auto &cache = _storage->getCache();
        for (auto &pr : cache) {
            onRead(pr.second, pr.first);
        }
    }

private:
    function<void(const T &)> _read_cb = [](const T &) {};
    function<void(void)> _detach_cb = []() {};
    shared_ptr<_RingStorage<T> > _storage;
    bool _use_cache;
};

template<typename T>
class _RingStorage {
public:
    typedef std::shared_ptr<_RingStorage> Ptr;

    _RingStorage(int max_size) {
        //gop缓存个数不能小于32
        if(max_size < RING_MIN_SIZE){
            max_size = RING_MIN_SIZE;
        }
        _max_size = max_size;
    }

    ~_RingStorage() {}

    /**
     * 写入环形缓存数据
     * @param in 数据
     * @param is_key 是否为关键帧
     * @return 是否触发重置环形缓存大小
     */
    inline void write(const T &in, bool is_key = true) {
        if (is_key) {
            //遇到I帧，那么移除老数据
            _data_cache.clear();
            _size = 0;
        }
        _data_cache.emplace_back(std::make_pair(is_key, std::move(in)));
        if (++_size > _max_size) {
            //GOP缓存溢出，需要移除老数据
            _data_cache.pop_front();
            --_size;
        }
    }

    Ptr clone() const {
        Ptr ret(new _RingStorage());
        ret->_data_cache = _data_cache;
        ret->_max_size = _max_size;
        ret->_size = _size;
        return ret;
    }

    const deque<pair<bool, T> > &getCache() const {
        return _data_cache;
    }
private:
    _RingStorage() = default;
private:
    deque<pair<bool, T> > _data_cache;
    int _max_size;
    int _size = 0;
};

template<typename T>
class RingBuffer;

/**
* 环形缓存事件派发器，只能一个poller线程操作它
* @tparam T
*/
template<typename T>
class _RingReaderDispatcher : public enable_shared_from_this<_RingReaderDispatcher<T> > {
public:
    typedef std::shared_ptr<_RingReaderDispatcher> Ptr;
    typedef _RingReader<T> RingReader;
    typedef _RingStorage<T> RingStorage;

    friend class RingBuffer<T>;

    ~_RingReaderDispatcher() {
        decltype(_reader_map) reader_map;
        reader_map.swap(_reader_map);
        for (auto &pr : reader_map) {
            auto reader = pr.second.lock();
            if (reader) {
                reader->onDetach();
            }
        }
    }

private:
    _RingReaderDispatcher(const typename RingStorage::Ptr &storage, const function<void(int, bool)> &onSizeChanged) {
        _storage = storage;
        _reader_size = 0;
        _on_size_changed = onSizeChanged;
    }

    void write(const T &in, bool is_key = true) {
        for (auto it = _reader_map.begin(); it != _reader_map.end();) {
            auto reader = it->second.lock();
            if (!reader) {
                it = _reader_map.erase(it);
                --_reader_size;
                onSizeChanged(false);
                continue;
            }
            reader->onRead(in, is_key);
            ++it;
        }
        _storage->write(in, is_key);
    }

    std::shared_ptr<RingReader> attach(const EventPoller::Ptr &poller, bool use_cache) {
        if (!poller->isCurrentThread()) {
            throw std::runtime_error("必须在绑定的poller线程中执行attach操作");
        }

        weak_ptr<_RingReaderDispatcher> weakSelf = this->shared_from_this();
        auto on_dealloc = [weakSelf, poller](RingReader *ptr) {
            poller->async([weakSelf, ptr]() {
                auto strongSelf = weakSelf.lock();
                if (strongSelf && strongSelf->_reader_map.erase(ptr)) {
                    --strongSelf->_reader_size;
                    strongSelf->onSizeChanged(false);
                }
                delete ptr;
            });
        };

        std::shared_ptr<RingReader> reader(new RingReader(_storage, use_cache), on_dealloc);
        _reader_map[reader.get()] = std::move(reader);
        ++_reader_size;
        onSizeChanged(true);
        return reader;
    }

    int readerCount() {
        return _reader_size;
    }

    void onSizeChanged(bool add_flag) {
        _on_size_changed(_reader_size, add_flag);
    }

private:
    function<void(int, bool)> _on_size_changed;
    atomic_int _reader_size;
    typename RingStorage::Ptr _storage;
    unordered_map<void *, std::weak_ptr<RingReader> > _reader_map;
};

template<typename T>
class RingBuffer : public enable_shared_from_this<RingBuffer<T> > {
public:
    typedef std::shared_ptr<RingBuffer> Ptr;
    typedef _RingReader<T> RingReader;
    typedef _RingStorage<T> RingStorage;
    typedef _RingReaderDispatcher<T> RingReaderDispatcher;
    typedef function<void(const EventPoller::Ptr &poller, int size, bool add_flag)> onReaderChanged;

    RingBuffer(int max_size = 1024, const onReaderChanged &cb = nullptr) {
        _on_reader_changed = cb;
        _storage = std::make_shared<RingStorage>(max_size);
    }

    ~RingBuffer() {}

    void write(const T &in, bool is_key = true) {
        if (_delegate) {
            _delegate->onWrite(in, is_key);
            return;
        }

        LOCK_GUARD(_mtx_map);
        _storage->write(in, is_key);
        for (auto &pr : _dispatcher_map) {
            auto &second = pr.second;
            pr.first->async([second, in, is_key]() {
                second->write(in, is_key);
            }, false);
        }
    }

    void setDelegate(const typename RingDelegate<T>::Ptr &delegate) {
        _delegate = delegate;
    }

    std::shared_ptr<RingReader> attach(const EventPoller::Ptr &poller, bool use_cache = true) {
        typename RingReaderDispatcher::Ptr dispatcher;
        {
            LOCK_GUARD(_mtx_map);
            auto &ref = _dispatcher_map[poller];
            if (!ref) {
                weak_ptr<RingBuffer> weakSelf = this->shared_from_this();
                auto onSizeChanged = [weakSelf, poller](int size, bool add_flag) {
                    auto strongSelf = weakSelf.lock();
                    if (!strongSelf) {
                        return;
                    }
                    strongSelf->onSizeChanged(poller, size, add_flag);
                };

                auto onDealloc = [poller](RingReaderDispatcher *ptr) {
                    poller->async([ptr]() {
                        delete ptr;
                    });
                };
                ref.reset(new RingReaderDispatcher(_storage->clone(), std::move(onSizeChanged)), std::move(onDealloc));
            }
            dispatcher = ref;
        }

        return dispatcher->attach(poller, use_cache);
    }

    int readerCount() {
        LOCK_GUARD(_mtx_map);
        int total = 0;
        for (auto &pr : _dispatcher_map) {
            total += pr.second->readerCount();
        }
        return total;
    }

private:
    void onSizeChanged(const EventPoller::Ptr &poller, int size, bool add_flag) {
        if (size == 0) {
            LOCK_GUARD(_mtx_map);
            _dispatcher_map.erase(poller);
        }

        if (_on_reader_changed) {
            _on_reader_changed(poller, size, add_flag);
        }
    }

private:
    struct HashOfPtr {
        std::size_t operator()(const EventPoller::Ptr &key) const {
            return (uint64_t) key.get();
        }
    };

private:
    mutex _mtx_map;
    typename RingStorage::Ptr _storage;
    typename RingDelegate<T>::Ptr _delegate;
    onReaderChanged _on_reader_changed;
    unordered_map<EventPoller::Ptr, typename RingReaderDispatcher::Ptr, HashOfPtr> _dispatcher_map;
};

} /* namespace toolkit */
#endif /* UTIL_RINGBUFFER_H_ */
