/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SQL_SQLCONNECTION_H_
#define SQL_SQLCONNECTION_H_

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include <deque>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include "Util/logger.h"
#include "Util/util.h"
#if defined(_WIN32)
#include <mysql.h>
#pragma  comment (lib,"libmysql") 
#else
#include <mysql/mysql.h>
#endif // defined(_WIN32)
using namespace std;

namespace toolkit {

/**
 * 数据库异常类
 */
class SqlException : public exception {
public:
    SqlException(const string &sql,const string &err){
        _sql = sql;
        _err = err;
    }
    virtual const char* what() const noexcept {
        return _err.data();
    }
    const string &getSql() const{
        return _sql;
    }
private:
    string _sql;
    string _err;
};

/**
 * mysql连接
 */
class SqlConnection {
public:
    /**
     * 构造函数
     * @param url 数据库地址
     * @param port 数据库端口号
     * @param dbname 数据库名
     * @param username 用户名
     * @param password 用户密码
     * @param character 字符集
     */
    SqlConnection(const string &url, unsigned short port,
                  const string &dbname, const string &username,
                  const string &password, const string &character = "utf8mb4") {
        mysql_init(&_sql);
        unsigned int timeout = 3;
        mysql_options(&_sql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
        if (!mysql_real_connect(&_sql, url.data(), username.data(),
                                password.data(), dbname.data(), port, NULL, 0)) {
            mysql_close(&_sql);
            throw SqlException("mysql_real_connect",mysql_error(&_sql));
        }
        //兼容bool与my_bool
        uint32_t reconnect = 0x01010101;
        mysql_options(&_sql, MYSQL_OPT_RECONNECT, &reconnect);
        mysql_set_character_set(&_sql, character.data());
    }
    ~SqlConnection(){
        mysql_close(&_sql);
    }


    /**
     * 以printf样式执行sql,无数据返回
     * @param rowId insert时的插入rowid
     * @param fmt printf类型fmt
     * @param arg 可变参数列表
     * @return 影响行数
     */
    template<typename Fmt,typename ...Args>
    int64_t query(int64_t &rowId, Fmt &&fmt, Args && ...arg) {
        check();
        auto tmp = queryString(std::forward<Fmt>(fmt), std::forward<Args>(arg)...);
        if (doQuery(tmp)) {
            throw SqlException(tmp,mysql_error(&_sql));
        }
        rowId=mysql_insert_id(&_sql);
        return mysql_affected_rows(&_sql);
    }

    /**
     * 以printf样式执行sql,并且返回list类型的结果(不包含数据列名)
     * @param rowId insert时的插入rowid
     * @param ret 返回数据列表
     * @param fmt printf类型fmt
     * @param arg 可变参数列表
     * @return 影响行数
     */
    template<typename Fmt,typename ...Args>
    int64_t query(int64_t &rowId,vector<vector<string> > &ret, Fmt &&fmt, Args && ...arg){
        return queryList(rowId,ret,std::forward<Fmt>(fmt),std::forward<Args>(arg)...);
    }
    template<typename Fmt,typename ...Args>
    int64_t query(int64_t &rowId,vector<list<string> > &ret, Fmt &&fmt, Args && ...arg){
        return queryList(rowId,ret,std::forward<Fmt>(fmt),std::forward<Args>(arg)...);
    }
    template<typename Fmt,typename ...Args>
    int64_t query(int64_t &rowId,vector<deque<string> > &ret, Fmt &&fmt, Args && ...arg){
        return queryList(rowId,ret,std::forward<Fmt>(fmt),std::forward<Args>(arg)...);
    }

    /**
     * 以printf样式执行sql,并且返回Map类型的结果(包含数据列名)
     * @param rowId insert时的插入rowid
     * @param ret 返回数据列表
     * @param fmt printf类型fmt
     * @param arg 可变参数列表
     * @return 影响行数
     */
    template<typename Map,typename Fmt,typename ...Args>
    int64_t query(int64_t &rowId,vector<Map> &ret, Fmt &&fmt, Args && ...arg) {
        check();
        auto tmp = queryString(std::forward<Fmt>(fmt), std::forward<Args>(arg)...);
        if (doQuery(tmp)) {
            throw SqlException(tmp,mysql_error(&_sql));
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

    string escape(const string &str) {
        char *out = new char[str.length() * 2 + 1];
        mysql_real_escape_string(&_sql, out, str.c_str(), str.size());
        string ret(out);
        delete [] out;
        return ret;
    }

    template<typename ...Args>
    static string queryString(const char *fmt, Args && ...arg) {
        char *ptr_out = NULL;
        asprintf(&ptr_out, fmt, arg...);
        if (ptr_out) {
            string ret(ptr_out);
            free(ptr_out);
            return ret;
        }
        return "";
    }

    template<typename ...Args>
    static string queryString(const string &fmt, Args && ...args) {
        return queryString(fmt.data(),std::forward<Args>(args)...);
    }
    static const char *queryString(const char *fmt) {
        return fmt;
    }
    static const string &queryString(const string &fmt) {
        return fmt;
    }
private:
    template<typename List,typename Fmt,typename ...Args>
    int64_t queryList(int64_t &rowId,vector<List> &ret, Fmt &&fmt, Args && ...arg) {
        check();
        auto tmp = queryString(std::forward<Fmt>(fmt), std::forward<Args>(arg)...);
        if (doQuery(tmp)) {
            throw SqlException(tmp,mysql_error(&_sql));
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

    inline void check() {
        if (mysql_ping(&_sql) != 0) {
            throw SqlException("mysql_ping","MYSQL连接异常!");
        }
    }

    int doQuery(const string &sql){
        return mysql_query(&_sql,sql.data());
    }
    int doQuery(const char *sql){
        return mysql_query(&_sql,sql);
    }
private:
    MYSQL _sql;
};

} /* namespace toolkit */
#endif /* SQL_SQLCONNECTION_H_ */
