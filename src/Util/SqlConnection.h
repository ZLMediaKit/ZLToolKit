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

#ifndef SQL_SQLCONNECTION_H_
#define SQL_SQLCONNECTION_H_

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include "Util/logger.h"

#if defined(_WIN32)
#include <mysql.h>
#pragma  comment (lib,"libmysql") 
#else
#include <mysql/mysql.h>
#endif // defined(_WIN32)


using namespace std;

namespace toolkit {

class SqlConnection {
public:
    SqlConnection(const string &url, unsigned short port,
                  const string &dbname, const string &username,
                  const string &password, const string &character = "utf8mb4") {
        mysql_init(&_sql);
        unsigned int timeout = 3;
        mysql_options(&_sql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
        if (!mysql_real_connect(&_sql, url.c_str(), username.c_str(),
                                password.c_str(), dbname.c_str(), port, NULL, 0)) {
            mysql_close(&_sql);
            throw runtime_error(string("mysql_real_connect:") + mysql_error(&_sql));
        }
        my_bool reconnect = 1;
        mysql_options(&_sql, MYSQL_OPT_RECONNECT, &reconnect);
        mysql_set_character_set(&_sql, character.data());
    }
	~SqlConnection(){
        mysql_close(&_sql);
    }

	template<typename ...Args>
	int64_t query(int64_t &rowId, const char *fmt, Args && ...arg) {
		check();
		string tmp = queryString(fmt, std::forward<Args>(arg)...);
		if (mysql_query(&_sql, tmp.c_str())) {
			WarnL << mysql_error(&_sql) << ":" << tmp << endl;
			return -1;
		}
		rowId=mysql_insert_id(&_sql);
		return mysql_affected_rows(&_sql);
	}

	int64_t query(int64_t &rowId,const char *str) {
		check();
		if (mysql_query(&_sql, str)) {
			WarnL << mysql_error(&_sql) << ":" << str << endl;
			return -1;
		}
		rowId=mysql_insert_id(&_sql);
		return mysql_affected_rows(&_sql);
	}
	template<typename ...Args>
	int64_t query(int64_t &rowId,vector<vector<string> > &ret, const char *fmt,
			Args && ...arg) {
		check();
		string tmp = queryString(fmt, std::forward<Args>(arg)...);
		if (mysql_query(&_sql, tmp.c_str())) {
			WarnL << mysql_error(&_sql)  << ":" << tmp << endl;
			return -1;
		}
		ret.clear();
		MYSQL_RES *res = mysql_store_result(&_sql);
		if (!res) {
			rowId=mysql_insert_id(&_sql);
			return mysql_affected_rows(&_sql);
		}
		MYSQL_ROW row;
		unsigned int column = mysql_num_fields(res);
		while ((row = mysql_fetch_row(res)) != NULL) {
			ret.emplace_back();
			auto &back = ret.back();
			for (unsigned int i = 0; i < column; i++) {
				back.emplace_back(row[i] ? row[i] : "");
			}
		}
		mysql_free_result(res);
		rowId=mysql_insert_id(&_sql);
		return mysql_affected_rows(&_sql);
	}

	template<typename Map,typename ...Args>
	int64_t query(int64_t &rowId,vector<Map> &ret, const char *fmt, Args && ...arg) {
		check();
		string tmp = queryString(fmt, std::forward<Args>(arg)...);
		if (mysql_query(&_sql, tmp.c_str())) {
			WarnL << mysql_error(&_sql)  << ":" << tmp << endl;
			return -1;
		}
		ret.clear();
		MYSQL_RES *res = mysql_store_result(&_sql);
		if (!res) {
			rowId=mysql_insert_id(&_sql);
			return mysql_affected_rows(&_sql);
		}
		MYSQL_ROW row;
		unsigned int column = mysql_num_fields(res);
		MYSQL_FIELD *fields = mysql_fetch_fields(res);
		while ((row = mysql_fetch_row(res)) != NULL) {
			ret.emplace_back();
			auto &back = ret.back();
			for (unsigned int i = 0; i < column; i++) {
				back[string(fields[i].name,fields[i].name_length)] = (row[i] ? row[i] : "");
			}
		}
		mysql_free_result(res);
		rowId=mysql_insert_id(&_sql);
		return mysql_affected_rows(&_sql);
	}
	int64_t query(int64_t &rowId,vector<vector<string>> &ret, const char *str) {
		check();
		if (mysql_query(&_sql, str)) {
			WarnL << mysql_error(&_sql)  << ":" << str << endl;
			return -1;
		}
		ret.clear();
		MYSQL_RES *res = mysql_store_result(&_sql);
		if (!res) {
			rowId=mysql_insert_id(&_sql);
			return mysql_affected_rows(&_sql);
		}
		MYSQL_ROW row;
		unsigned int column = mysql_num_fields(res);
		while ((row = mysql_fetch_row(res)) != NULL) {
			ret.emplace_back();
			auto &back = ret.back();
			for (unsigned int i = 0; i < column; i++) {
				back.emplace_back(row[i] ? row[i] : "" );
			}
		}
		mysql_free_result(res);
		rowId=mysql_insert_id(&_sql);
		return mysql_affected_rows(&_sql);
	}
	template<typename ...Args>
	static string queryString(const char *fmt, Args && ...arg) {
		char *ptr_out = NULL;
		asprintf(&ptr_out, fmt, arg...);
		string ret(ptr_out);
		if (ptr_out) {
			free(ptr_out);
		}
		return ret;
	}
	static string queryString(const char *fmt) {
		return fmt;
	}
	string &escape(string &str) {
		char *out = new char[str.length() * 2 + 1];
		mysql_real_escape_string(&_sql, out, str.c_str(), str.size());
		str.assign(out);
		delete [] out;
		return str;
	}
private:
	MYSQL _sql;
	inline void check() {
		if (mysql_ping(&_sql) != 0) {
			throw runtime_error("MYSQL连接异常!");
		}
	}
};

} /* namespace toolkit */
#endif /* SQL_SQLCONNECTION_H_ */
