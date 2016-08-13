/*
 * SqlPool.h
 *
 *  Created on: 2015年10月29日
 *      Author: root
 */

#ifndef SQL_SQLPOOL_H_
#define SQL_SQLPOOL_H_
#include <Util/logger.h>
#include <Util/recyclePool.h>
#include <Util/SqlConnection.h>
#include "Thread/AsyncTaskThread.h"
#include "Thread/ThreadPool.hpp"
#include <memory>
#include <mutex>
#include <functional>
#include <sstream>

using namespace std;
using namespace ZL::Thread;
namespace ZL {
namespace Util {
template<int poolSize = 10>
class _SqlPool {
public:
	typedef recyclePool<SqlConnection, poolSize> PoolType;
	typedef vector<vector<string> > SqlRetType;
	static _SqlPool &Instance() {
		static _SqlPool pool;
		return pool;
	}
	void setPoolSize(int size) {
		if (size < 0) {
			return;
		}
		poolsize = size;
		pool->setPoolSize(size);
		threadPool.reset(new ThreadPool(poolsize));
	}
	template<typename ...Args>
	void Init(Args && ...arg) {
		pool.reset(new PoolType(arg...));
		pool->obtain();
	}

	template<typename ...Args>
	int64_t query(const char *fmt, Args && ...arg) {
		string sql = SqlConnection::queryString(fmt, arg...);
		doQuery(sql);
		return 0;
	}
	int64_t query(const string &sql) {
		doQuery(sql);
		return 0;
	}

	template<typename ...Args>
	int64_t query(vector<vector<string>> &ret, const char *fmt,
			Args && ...arg) {
		return _query(ret, fmt, arg...);
	}

	int64_t query(vector<vector<string>> &ret, const string &sql) {
		return _query(ret, sql.c_str());
	}
	static const string &escape(const string &str) {
		try {
			//捕获创建对象异常
			_SqlPool::Instance().pool->obtain()->escape(
					const_cast<string &>(str));
		} catch (exception &e) {
			WarnL << e.what() << endl;
			(const_cast<string &>(str)).clear();
		}
		return str;
	}

private:
	_SqlPool() :
			threadPool(new ThreadPool(poolSize)), asyncTaskThread(10 * 1000) {
		poolsize = poolSize;
		asyncTaskThread.DoTaskDelay(reinterpret_cast<uint64_t>(this), 30 * 1000,
				[this]() {
					flushError();
					return true;
				});
	}
	inline void doQuery(const string &str) {
		auto lam = [this,str]() {
			if(_query(str.c_str())==-1) {
				lock_guard<mutex> lk(error_query_mutex);
				error_query.push_back(str);
			}
		};
		threadPool->post(lam);
	}
	template<typename ...ARGS>
	inline int64_t _query(ARGS &&...args) {
		try {
			//捕获创建对象异常
			auto mysql = pool->obtain();
			try {
				//捕获执行异常
				return mysql->query(args...);
			} catch (exception &e) {
				mysql.quit();
				WarnL << e.what() << endl;
				return -1;
			}
		} catch (exception &e) {
			WarnL << e.what() << endl;
			return -1;
		}
	}
	void flushError() {
		shared_ptr<list<string> > query_new(new list<string>());
		error_query_mutex.lock();
		query_new->swap(error_query);
		error_query_mutex.unlock();
		if (query_new->size() == 0) {
			return;
		}
		for (auto &sql : *(query_new.get())) {
			doQuery(sql);
		}
	}
	virtual ~_SqlPool() {
		asyncTaskThread.CancelTask(reinterpret_cast<uint64_t>(this));
		flushError();
	}
	shared_ptr<ThreadPool> threadPool;
	mutex error_query_mutex;
	list<string> error_query;

	shared_ptr<PoolType> pool;
	AsyncTaskThread asyncTaskThread;
	unsigned int poolsize;
}
;
typedef _SqlPool<1> SqlPool;

class _SqlStream {
public:
	_SqlStream(const char *_sql) :
			sql(_sql) {
		startPos = 0;
	}
	~_SqlStream() {

	}

	template<typename T>
	_SqlStream& operator <<(const T& data) {
		auto pos = sql.find_first_of('?', startPos);
		if (pos == string::npos) {
			return *this;
		}
		str_tmp.str("");
		str_tmp << data;
		string str = str_tmp.str();
		startPos = pos + str.size();
		sql.replace(pos, 1, str);
		return *this;
	}
	const string& operator <<(std::ostream&(*f)(std::ostream&)) const {
		return sql;
	}
private:
	stringstream str_tmp;
	string sql;
	string::size_type startPos;
};

class _SqlWriter {
public:
	_SqlWriter(const char *_sql) :
			sqlstream(_sql) {
	}
	~_SqlWriter() {

	}
	template<typename T>
	_SqlWriter& operator <<(const T& data) {
		sqlstream << data;
		return *this;
	}

	void operator <<(std::ostream&(*f)(std::ostream&)) {
		SqlPool::Instance().query(sqlstream << endl);
	}
	int64_t operator <<(vector<vector<string>> &ret) {
		return SqlPool::Instance().query(ret, sqlstream << endl);
	}
private:
	_SqlStream sqlstream;
};

#define SqlWriter  _SqlWriter
#define SqlStream  _SqlStream
} /* namespace mysql */
} /* namespace im */

#endif /* SQL_SQLPOOL_H_ */
