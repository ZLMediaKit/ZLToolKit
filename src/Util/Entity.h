/*
 * Entity.h
 *
 *  Created on: 2017年3月16日
 *      Author: xzl
 */

#ifndef SRC_UTIL_ENTITY_H_
#define SRC_UTIL_ENTITY_H_

#include <list>
#include <memory>
#include "util.h"
#include <type_traits>
#include "logger.h"
#include <vector>
#include <typeindex>

using namespace std;

namespace ZL {
namespace Util {

class Entity {
public:
	typedef std::shared_ptr<Entity> Ptr;
	Entity() {
	}

	template<typename T>
	Entity(const T &t) {
		_assin(t);
	}
	template<typename T>
	Entity(T &&t) {
		_assin(std::forward<T>(t));
	}

	virtual ~Entity() {
		if (_ptr) {
			_del(_ptr);
		}
	}
	Entity & operator=(const Entity &that) {
		_alloc = that._alloc;
		_del = that._del;
		if (that._ptr) {
			_ptr = _alloc(that._ptr);
		}
		_name = that._name;
		return *this;
	}

	Entity & operator=(Entity &&that) {
		allocor alloc = _alloc;
		deletor del = _del;
		void *ptr = _ptr;
		const char *name = _name;

		_alloc = that._alloc;
		_del = that._del;
		_ptr = that._ptr;
		_name = that._name;

		that._alloc = alloc;
		that._del = del;
		that._ptr = ptr;
		that._name = name;

		return *this;
	}

	template<typename T>
	typename std::remove_reference<T>::type &as() {
		typedef typename std::remove_reference<T>::type Type;
		if (!_ptr || !_name) {
			throw std::runtime_error("Entity: nullptr point");
		}
		if (_name != typeid(Type).name()) {
			auto err = StrPrinter << "Entity: type mismatch (" << _name << "!="
					<< typeid(Type).name() << ")" << endl;
			throw std::runtime_error(err);
		}
		Type *ret = static_cast<Type*>(_ptr);
		return *ret;
	}

	template<typename T>
	const typename std::remove_reference<T>::type &as() const {
		return ((Entity *) this)->as<T>();
	}

private:
	typedef void *(*allocor)(void *t);
	typedef void (*deletor)(void *ptr);
	allocor _alloc = nullptr;
	deletor _del = nullptr;
	void *_ptr = nullptr;
	const char *_name = nullptr;

	template<typename T>
	typename std::enable_if<
	!std::is_same<Entity, typename std::remove_reference<T>::type>::value,
	void>::type _assin(const T &t) {
		// copy other type
		using Type = typename std::remove_reference<T>::type;
		if (typeid(Type) == typeid(Entity)) {
			throw std::invalid_argument("Entity:store a Entity");
		}
		_alloc = [] (void *ptr) {
			Type *tPtr = (Type *)ptr;
			return (void *)new Type(*tPtr);
		};
		_del = [](void *ptr) {
			delete (Type *)ptr;
		};
		_ptr = _alloc((void *) &t);
		_name = typeid(Type).name();

	}

	template<typename T>
	typename std::enable_if<
	!std::is_same<Entity, typename std::remove_reference<T>::type>::value,
	void>::type _assin(T &&t) {
		// move other type
		using Type = typename std::remove_reference<T>::type;
		if (typeid(Type) == typeid(Entity)) {
			throw std::invalid_argument("Entity:store a Entity");
		}
		_alloc = [] (void *ptr) {
			Type *tPtr = (Type *)ptr;
			return (void *)new Type(std::move(*tPtr));
		};
		_del = [](void *ptr) {
			delete (Type *)ptr;
		};
		_ptr = _alloc((void *) &t);
		_name = typeid(Type).name();
	}

	template<typename T>
	typename std::enable_if<
	std::is_same<Entity, typename std::remove_reference<T>::type>::value,
	void>::type _assin(const T &t) {
		// copy this type
		(*this) = t;
	}

	template<typename T>
	typename std::enable_if<
	std::is_same<Entity, typename std::remove_reference<T>::type>::value,
	void>::type _assin(T &&t) {
		// move this type
		(*this) = std::forward<T>(t);
	}
};

class EntityInterface {
public:
	virtual Entity &operator[](int idex) = 0;
	virtual const Entity &operator[](int idex) const = 0;
	virtual ~EntityInterface() { }
	virtual int size() const = 0;
	virtual const std::vector<Entity>* operator->() const = 0 ;
	virtual std::vector<Entity>* operator->() = 0;
protected:
	virtual void inner_push_front(Entity &&ent) = 0;
};

template<typename ... ArgTypes>
class _MultiEntity;

template<typename T, typename ... ArgTypes>
class _MultiEntity<T, ArgTypes...> : public _MultiEntity<ArgTypes...>,
virtual public EntityInterface {
public:
	_MultiEntity(typename std::remove_reference<T>::type &&t,
			ArgTypes &&...args) :
				_MultiEntity<ArgTypes...>(std::forward<ArgTypes>(args)...) {
		inner_push_front(Entity(std::forward<T>(t)));
	}
	_MultiEntity(const typename std::remove_reference<T>::type &t,
			ArgTypes &&...args) :
				_MultiEntity<ArgTypes...>(std::forward<ArgTypes>(args)...) {
		inner_push_front(Entity(t));
	}
};

template<typename T>
class _MultiEntity<T> : virtual public EntityInterface {
public:
	_MultiEntity(const typename std::remove_reference<T>::type &t) {
		inner_push_front(Entity(t));
	}
	_MultiEntity(typename std::remove_reference<T>::type &&t) {
		inner_push_front(Entity(std::forward<T>(t)));
	}

	Entity &operator[](int idex) override {
		if (idex < 0 || idex >= (int) _entList.size()) {
			throw std::out_of_range("MultiEntity : operator[]");
		}
		return _entList[idex];
	}
	const Entity &operator[](int idex) const override {
		if (idex < 0 || idex >= (int) _entList.size()) {
			throw std::out_of_range("MultiEntity : operator[]");
		}
		return _entList[idex];
	}

	int size() const override {
		return _entList.size();
	}
	const std::vector<Entity>* operator->() const override{
		return &_entList;
	}
	std::vector<Entity>* operator->() override{
		return &_entList;
	}
protected:
	void inner_push_front(Entity &&ent) override {
		_entList.insert(_entList.begin(),std::forward<Entity>(ent));
	}
private:
	std::vector<Entity> _entList;
};

class MultiEntity {
public:
	template<typename ... ArgTypes>
	MultiEntity(ArgTypes &&...args) {
		typedef _MultiEntity<ArgTypes...> dataType;
		_data.reset(new dataType(std::forward<ArgTypes>(args)...),
				[](void *ptr) {
			delete (dataType *)(ptr);
		});
	}

	Entity &operator[](int idex) {
		EntityInterface *ptr = (EntityInterface *) _data.get();
		return (*ptr)[idex];
	}
	const Entity &operator[](int idex) const {
		EntityInterface *ptr = (EntityInterface *) _data.get();
		return (*ptr)[idex];
	}
	int size() const {
		EntityInterface *ptr = (EntityInterface *) _data.get();
		return ptr->size();
	}
	const std::vector<Entity>* operator->() const {
		EntityInterface *ptr = (EntityInterface *) _data.get();
		return (*ptr).operator ->();
	}
	std::vector<Entity>* operator->() {
		EntityInterface *ptr = (EntityInterface *) _data.get();
		return (*ptr).operator ->();
	}
private:
	std::shared_ptr<void> _data;
};
}
/* namespace Util */
} /* namespace ZL */

#endif /* SRC_UTIL_ENTITY_H_ */
