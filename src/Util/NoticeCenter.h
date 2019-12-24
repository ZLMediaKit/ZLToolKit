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

#ifndef SRC_UTIL_NOTICECENTER_H_
#define SRC_UTIL_NOTICECENTER_H_

#include <mutex>
#include <memory>
#include <string>
#include <exception>
#include <functional>
#include <unordered_map>
#include <thread>
#include "function_traits.h"
#include "onceToken.h"
using namespace std;

namespace toolkit {

class NoticeCenter : public std::enable_shared_from_this<NoticeCenter>{
public:
	class InterruptException : public std::runtime_error {
	public:
		InterruptException():std::runtime_error("InterruptException"){}
		~InterruptException(){}
	};

	typedef unordered_map<string,unordered_multimap<void *,std::shared_ptr<void> > > MapType;
	typedef std::shared_ptr<NoticeCenter> Ptr;

	~NoticeCenter(){}
	static NoticeCenter &Instance();

    template<typename ...ArgsType>
    bool emitEvent(const string &strEvent,ArgsType &&...args){
		onceToken token([&] {
			//上锁，记录锁定线程id
			_mtxListener.lock();
			if(_lock_depth++ == 0){
				_lock_thread = this_thread::get_id();
			}
		}, [&]() {
			//释放锁，取消锁定线程id
			if(--_lock_depth == 0){
				_lock_thread = thread::id();
				if(_map_moved){
					//还原_mapListener
					_map_moved = false;
					_mapListener = std::move(_mapListenerTemp);
				}
			}
			_mtxListener.unlock();
		});

		auto it0 = _mapListener.find(strEvent);
		if (it0 == _mapListener.end()) {
			return false;
		}
		auto &listenerMap = it0->second;
		for (auto &pr : listenerMap) {
			typedef function<void(decltype(std::forward<ArgsType>(args))...)> funType;
			funType *obj = (funType *) (pr.second.get());
			try {
				(*obj)(std::forward<ArgsType>(args)...);
			} catch (InterruptException &ex) {
				break;
			}
		}
		return listenerMap.size();
    }

	template<typename FUN>
	void addListener(void *tag, const string &strEvent, const FUN &fun) {
		typedef typename function_traits<FUN>::stl_function_type funType;
		std::shared_ptr<void> pListener(new funType(fun), [](void *ptr) {
			funType *obj = (funType *) ptr;
			delete obj;
		});
		lock_guard<recursive_mutex> lck(_mtxListener);
		getListenerMap()[strEvent].emplace(tag, pListener);
	}


	void delListener(void *tag,const string &strEvent){
		lock_guard<recursive_mutex> lck(_mtxListener);
		auto &listenerMap = getListenerMap();
		auto it = listenerMap.find(strEvent);
		if (it == listenerMap.end()) {
			return;
		}
		it->second.erase(tag);
		if (it->second.empty()) {
			listenerMap.erase(it);
		}
	}
	void delListener(void *tag){
		lock_guard<recursive_mutex> lck(_mtxListener);
		auto &listenerMap = getListenerMap();
		for (auto it = listenerMap.begin(); it != listenerMap.end();) {
			it->second.erase(tag);
			if (it->second.empty()) {
				it = listenerMap.erase(it);
				continue;
			}
			++it;
		}
	}

	void clearAll(){
		lock_guard<recursive_mutex> lck(_mtxListener);
		getListenerMap().clear();
    }
private:
	NoticeCenter(){}
	MapType &getListenerMap(){
		if(_lock_thread != this_thread::get_id()){
			//本次操作并不包含在emitEvent函数栈生命周期内
			return _mapListener;
		}

		//在emitEvent函数中执行_mapListener遍历操作的同时，还可能在回调中对_mapListener进行操作
		//所以我们要先把_mapListener拷贝到一个临时变量然后操作之(防止遍历时做删除或添加操作)
		if(_map_moved){
			//对象已经移动至_mapListenerTemp，所以请勿重复移动并覆盖
			return _mapListenerTemp;
		}
		//_mapListener移动到临时变量，在emitEvent函数中触发的回调中，操作的是这个临时变量：_mapListenerTemp
		_map_moved = true;
		_mapListenerTemp = _mapListener;
		return _mapListenerTemp;
	}
private:
	bool _map_moved = false;
	int _lock_depth = 0;
	thread::id _lock_thread;
	recursive_mutex _mtxListener;
	MapType _mapListener;
	MapType _mapListenerTemp;
};

} /* namespace toolkit */

#endif /* SRC_UTIL_NOTICECENTER_H_ */
