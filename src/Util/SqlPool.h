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

#ifndef SQL_SQLPOOL_H_
#define SQL_SQLPOOL_H_

#include <deque>
#include <mutex>
#include <memory>
#include <sstream>
#include <functional>
#include "logger.h"
#include "SqlConnection.h"
#include "Thread/ThreadPool.h"
#include "Util/ResourcePool.h"
#include "Thread/AsyncTaskThread.h"

using namespace std;

namespace toolkit {

class SqlPool {
public:
	typedef ResourcePool<SqlConnection> PoolType;
	typedef vector<vector<string> > SqlRetType;

	static SqlPool &Instance();
	static void Destory();

	~SqlPool() {
		AsyncTaskThread::Instance().CancelTask(reinterpret_cast<uint64_t>(this));
		flushError();
		_threadPool.reset();
		_pool.reset();
		InfoL;
	}
	
	void setSize(int size) {
		checkInited();
		_pool->setSize(size);
	}
	template<typename ...Args>
	void Init(Args && ...arg) {
		_pool.reset(new PoolType(std::forward<Args>(arg)...));
		_pool->obtain();
	}

	template<typename ...Args>
	int64_t query(const char *fmt, Args && ...arg) {
		string sql = SqlConnection::queryString(fmt, std::forward<Args>(arg)...);
		doQuery(sql);
		return 0;
	}
	int64_t query(const string &sql) {
		doQuery(sql);
		return 0;
	}

	template<typename Row,typename ...Args>
	int64_t query(int64_t &rowID,vector<Row> &ret, const char *fmt,
			Args && ...arg) {
		return _query(rowID,ret, fmt, std::forward<Args>(arg)...);
	}

	template<typename Row>
	int64_t query(int64_t &rowID,vector<Row> &ret, const string &sql) {
		return _query(rowID,ret, sql.c_str());
	}
	static const string &escape(const string &str) {
		SqlPool::Instance().checkInited();
		try {
			//捕获创建对象异常
			SqlPool::Instance()._pool->obtain()->escape(const_cast<string &>(str));
		} catch (exception &e) {
			WarnL << e.what() << endl;
		}
		return str;
	}

private:
	SqlPool() : _threadPool(new ThreadPool(1)) {
		AsyncTaskThread::Instance().DoTaskDelay(reinterpret_cast<uint64_t>(this), 30 * 1000,[this]() {
			flushError();
			return true;
		});
	}
	inline void doQuery(const string &str,int tryCnt = 3) {
		auto lam = [this,str,tryCnt]() {
			int64_t rowID;
			auto cnt = tryCnt - 1;
			if(_query(rowID,str.c_str())==-2 && cnt > 0) {
				lock_guard<mutex> lk(_error_query_mutex);
				sqlQuery query(str,cnt);
				_error_query.push_back(query);
			}
		};
		_threadPool->async(lam);
	}
	template<typename ...Args>
	inline int64_t _query(int64_t &rowID,Args &&...arg) {
		checkInited();
		typename PoolType::ValuePtr mysql;
		try {
			//捕获执行异常
			mysql = _pool->obtain();
			return mysql->query(rowID,std::forward<Args>(arg)...);
		} catch (exception &e) {
			_pool->quit(mysql);
			WarnL << e.what() << endl;
			return -2;
		}
	}
	void flushError() {
		decltype(_error_query) query_copy;
		_error_query_mutex.lock();
		query_copy.swap(_error_query);
		_error_query_mutex.unlock();
		if (query_copy.size() == 0) {
			return;
		}
		for (auto &query : query_copy) {
			doQuery(query.sql_str,query.tryCnt);
		}
	}

	void checkInited(){
		if(!_pool){
			throw std::runtime_error("请先调用SqlPool::Init初始化数据库连接池");
		}
	}
private:
	struct sqlQuery {
		sqlQuery(const string &sql,int cnt):sql_str(sql),tryCnt(cnt){}
		string sql_str;
		int tryCnt = 0;
	} ;

private:
	deque<sqlQuery> _error_query;
	std::shared_ptr<ThreadPool> _threadPool;
	mutex _error_query_mutex;
	std::shared_ptr<PoolType> _pool;
}
;

class SqlStream {
public:
	SqlStream(const char *sql) :
			_sql(sql) {
		_startPos = 0;
	}
	~SqlStream() {

	}

	template<typename T>
	SqlStream& operator <<(const T& data) {
		auto pos = _sql.find('?', _startPos);
		if (pos == string::npos) {
			return *this;
		}
		_str_tmp.str("");
		_str_tmp << data;
		string str = SqlPool::escape(_str_tmp.str());
		_startPos = pos + str.size();
		_sql.replace(pos, 1, str);
		return *this;
	}
	const string& operator <<(std::ostream&(*f)(std::ostream&)) const {
		return _sql;
	}
	operator string (){
		return _sql;
	}
private:
	stringstream _str_tmp;
	string _sql;
	string::size_type _startPos;
};

class SqlWriter {
public:
	SqlWriter(const char *sql,bool throwAble = true) :
			_sqlstream(sql),_throwAble(throwAble) {
	}
	~SqlWriter() {

	}
	template<typename T>
	SqlWriter& operator <<(const T& data) {
		_sqlstream << data;
		return *this;
	}

	void operator <<(std::ostream&(*f)(std::ostream&)) {
		SqlPool::Instance().query(_sqlstream << endl);
	}
	template <typename Row>
	int64_t operator <<(vector<Row> &ret) {
		_affectedRows = SqlPool::Instance().query(_rowId,ret, _sqlstream << endl);
		if(_affectedRows < 0 && _throwAble){
			throw std::runtime_error("operate database failed");
		}
		return _affectedRows;
	}
	int64_t getRowID() const {
		return _rowId;
	}

	int64_t getAffectedRows() const {
		return _affectedRows;
	}

private:
	SqlStream _sqlstream;
	int64_t _rowId = 0;
	int64_t _affectedRows = -1;
	bool _throwAble = true;
};

} /* namespace toolkit */

#endif /* SQL_SQLPOOL_H_ */
