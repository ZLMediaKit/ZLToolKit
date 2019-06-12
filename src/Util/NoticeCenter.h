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
#include "function_traits.h"

using namespace std;

namespace toolkit {


class NoticeCenter {
public:
	class InterruptException : public std::runtime_error
	{
	public:
		InterruptException():std::runtime_error("InterruptException"){}
		~InterruptException(){}
	};

	~NoticeCenter(){}
	static NoticeCenter &Instance();

	template<typename ...ArgsType>
	bool emitEvent(const string &strEvent,ArgsType &&...args){
		decltype(_mapListener)::mapped_type listenerMap;
		{
			lock_guard<recursive_mutex> lck(_mtxListener);
			auto it0 = _mapListener.find(strEvent);
			if (it0 == _mapListener.end()) {
				return false;
			}
			listenerMap = it0->second;
		}
		for(auto &pr : listenerMap){
			typedef function<void(decltype(std::forward<ArgsType>(args))...)> funType;
			funType *obj = (funType *)(pr.second.get());
			try{
				(*obj)(std::forward<ArgsType>(args)...);
			}catch(InterruptException &ex){
				break;
			}
		}
		return listenerMap.size();
	}

    template<typename ...ArgsType>
    bool emitEventNoCopy(const string &strEvent,ArgsType &&...args){
        lock_guard<recursive_mutex> lck(_mtxListener);
        auto it0 = _mapListener.find(strEvent);
        if (it0 == _mapListener.end()) {
            return false;
        }
        auto &listenerMap = it0->second;
        for(auto &pr : listenerMap){
            typedef function<void(decltype(std::forward<ArgsType>(args))...)> funType;
            funType *obj = (funType *)(pr.second.get());
            try{
                (*obj)(std::forward<ArgsType>(args)...);
            }catch(InterruptException &ex){
                break;
            }
        }
        return listenerMap.size();
    }

	int listenerSize(const string &strEvent){
        lock_guard<recursive_mutex> lck(_mtxListener);
        auto it0 = _mapListener.find(strEvent);
        if (it0 == _mapListener.end()) {
            return 0;
        }
        return it0->second.size();
    }


	template<typename FUN>
	void addListener(void *tag, const string &strEvent, const FUN &fun) {
		typedef typename function_traits<FUN>::stl_function_type funType;
		std::shared_ptr<void> pListener(new funType(fun), [](void *ptr) {
			funType *obj = (funType *)ptr;
			delete obj;
		});
		lock_guard<recursive_mutex> lck(_mtxListener);
		_mapListener[strEvent].emplace(tag,pListener);
	}


	void delListener(void *tag,const string &strEvent){
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

	void clearAll(){
        lock_guard<recursive_mutex> lck(_mtxListener);
        _mapListener.clear();
    }
private:
	NoticeCenter(){}
	recursive_mutex _mtxListener;
	unordered_map<string,unordered_multimap<void *,std::shared_ptr<void> > > _mapListener;

};

} /* namespace toolkit */

#endif /* SRC_UTIL_NOTICECENTER_H_ */
