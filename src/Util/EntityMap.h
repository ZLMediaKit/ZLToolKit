/*
 * ConfigProvider.h
 *
 *  Created on: 2017年3月14日
 *      Author: xzl
 */

#ifndef SRC_UTIL_ENTITYMAP_H_
#define SRC_UTIL_ENTITYMAP_H_

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <tuple>
#include "util.h"
#include "function_traits.h"
#include "logger.h"


using namespace std;
using namespace ZL::Util;

namespace ZL {
namespace Util {

class EntityMap {
public:
	virtual ~EntityMap() {
	}
	EntityMap() {
	}
	static EntityMap &Instance() {
		static EntityMap instance;
		return instance;
	}
	template<typename RetType>
	const RetType &get(const char *cfgName) {
		const RetType *ret;
		getAll(cfgName, [&] (const RetType &arg) {
			ret = &arg;
		});
		return *ret;
	}

	template<typename ...ArgsType>
	void set(const char *cfgName, ArgsType &&...cfgs) {
		typedef function<void(const typename std::remove_reference<ArgsType>::type &...)> invokeType;
		typedef function<void(const invokeType &)> providerType;
		std::shared_ptr<void> provider((void *) new providerType([=](const invokeType &invoker) {
			invoker(cfgs...);
		}),
		[](void *ptr) {
			delete (providerType *)ptr;
		});
		lock_guard<mutex> lck(_mtx);
		_providerMap.emplace(cfgName, provider);
	}

	template<typename FUN>
	bool getAll(const char *cfgName, const FUN &onGet) {
		std::shared_ptr<void> got;
		{
			lock_guard<mutex> lck(_mtx);
			auto it = _providerMap.find(cfgName);
			if (it == _providerMap.end()) {
				return false;
			}
			got = it->second;
		}

		typedef typename function_traits<FUN>::stl_function_type invokeType;
		typedef function<void(const invokeType &)> providerType;
		providerType *provider = (providerType *) got.get();
		invokeType func(onGet);
		(*provider)(func);
		return true;
	}
	void clear(){
		lock_guard<mutex> lck(_mtx);
		_providerMap.clear();
	}
private:

	mutex _mtx;
	unordered_map<string, std::shared_ptr<void> > _providerMap;
};

} /* namespace Util */
} /* namespace ZL */

#endif /* SRC_UTIL_ENTITYMAP_H_ */
