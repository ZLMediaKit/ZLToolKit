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

#ifndef SRC_UTIL_NOTICECENTER_H_
#define SRC_UTIL_NOTICECENTER_H_

#include <mutex>
#include <memory>
#include <string>
#include <exception>
#include <functional>
#include <unordered_map>
#include "function_traits.h"

using namespace std;

namespace ZL {
namespace Util {


class NoticeCenter {
public:
	class InterruptException : public std::runtime_error
	{
	public:
		InterruptException():std::runtime_error("InterruptException"){}
		virtual ~InterruptException(){}
	};

	virtual ~NoticeCenter(){}
	static NoticeCenter &Instance(){
		static NoticeCenter instance;
		return instance;
	}
	template<typename ...ArgsType>
	bool emitEvent(const char *strEvent,ArgsType &&...args){
		lock_guard<recursive_mutex> lck(_mtxListener);
		auto it0 = _mapListener.find(strEvent);
		if (it0 == _mapListener.end()) {
			return false;
		}
		for(auto &pr : it0->second){
			typedef function<void(ArgsType &&...)> funType;
			funType *obj = (funType *)(pr.second.get());
			try{
				(*obj)(std::forward<ArgsType>(args)...);
			}catch(InterruptException &ex){
				break;
			}
		}
		return it0->second.size();
	}


	template<typename FUN>
	void addListener(void *tag, const char *strEvent, const FUN &fun) {
		typedef typename function_traits<FUN>::stl_function_type funType;
		std::shared_ptr<void> pListener(new funType(fun), [](void *ptr) {
			funType *obj = (funType *)ptr;
			delete obj;
		});
		lock_guard<recursive_mutex> lck(_mtxListener);
		_mapListener[strEvent].emplace(tag,pListener);
	}


	void delListener(void *tag,const char *strEvent){
		lock_guard<recursive_mutex> lck(_mtxListener);
		auto it = _mapListener.find(strEvent);
		if(it == _mapListener.end()){
			return;
		}
		it->second.erase(tag);
		if(it->second.empty()){
			_mapListener.erase(it);
		}
	}
	void delListener(void *tag){
		lock_guard<recursive_mutex> lck(_mtxListener);
		for(auto it = _mapListener.begin();it != _mapListener.end();){
			it->second.erase(tag);
			if(it->second.empty()){
				it = _mapListener.erase(it);
				continue;
			}
			++it;
		}
	}

private:
	NoticeCenter(){}
	recursive_mutex _mtxListener;
	unordered_map<string,unordered_multimap<void *,std::shared_ptr<void> > > _mapListener;

};

} /* namespace Util */
} /* namespace ZL */

#endif /* SRC_UTIL_NOTICECENTER_H_ */
