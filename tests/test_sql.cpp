/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include "Util/logger.h"
#if defined(ENABLE_MYSQL)
#include "Util/SqlPool.h"
#endif
using namespace std;
using namespace toolkit;

int main() {
    //初始化日志  [AUTO-TRANSLATED:371bb4e5]
    // Initialize log
    Logger::Instance().add(std::make_shared<ConsoleChannel> ());

#if defined(ENABLE_MYSQL)
    /*
     * 测试方法:
     * 请按照实际数据库情况修改源码然后编译执行测试
     /*
     * Test method:
     * Please modify the source code according to the actual database situation and then compile and run the test
     
     * [AUTO-TRANSLATED:0bf528e2]
     */
    string sql_ip = "127.0.0.1";
    unsigned short sql_port = 3306;
    string user = "root";
    string password = "111111";
    string character = "utf8mb4";

#if defined(SUPPORT_DYNAMIC_TEMPLATE)
    //初始化数据  [AUTO-TRANSLATED:62fabcf5]
    // Initialize data
    SqlPool::Instance().Init(sql_ip,sql_port,"",user,password/*,character*/);
#else
    //由于需要编译器对可变参数模板的支持，所以gcc5.0以下一般都不支持，否则编译报错  [AUTO-TRANSLATED:fde51ba6]
    // Because compiler support for variable parameter templates is required, versions below gcc5.0 generally do not support it, otherwise a compilation error will occur
    ErrorL << "your compiler does not support variable parameter templates!" << endl;
    return -1;
#endif //defined(SUPPORT_DYNAMIC_TEMPLATE)

    //建议数据库连接池大小设置跟CPU个数一致(大一点为佳)  [AUTO-TRANSLATED:14ca5335]
    // It is recommended to set the database connection pool size to be consistent with the number of CPUs (slightly larger is better)
    SqlPool::Instance().setSize(3 + thread::hardware_concurrency());

    vector<vector<string> > sqlRet;
    SqlWriter("create database test_db;", false) << sqlRet;
    SqlWriter("create table test_db.test_table(user_name  varchar(128),user_id int auto_increment primary key,user_pwd varchar(128));", false) << sqlRet;

    //同步插入  [AUTO-TRANSLATED:0c010898]
    // Synchronous insertion
    SqlWriter insertSql("insert into test_db.test_table(user_name,user_pwd) values('?','?');");
    insertSql<< "zltoolkit" << "123456" << sqlRet;
    //我们可以知道插入了几条数据，并且可以获取新插入(第一条)数据的rowID  [AUTO-TRANSLATED:6c3fe4ae]
    // We can know how many pieces of data were inserted, and we can get the rowID of the newly inserted (first) data
    DebugL << "AffectedRows:" << insertSql.getAffectedRows() << ",RowID:" << insertSql.getRowID();

    //同步查询  [AUTO-TRANSLATED:6f60ace1]
    // Synchronous query
    SqlWriter sqlSelect("select user_id , user_pwd from test_db.test_table where user_name='?' limit 1;") ;
    sqlSelect << "zltoolkit" ;

    vector<vector<string> > sqlRet0;
    vector<list<string> > sqlRet1;
    vector<deque<string> > sqlRet2;
    vector<map<string,string> > sqlRet3;
    vector<unordered_map<string,string> > sqlRet4;
    sqlSelect << sqlRet0;
    sqlSelect << sqlRet1;
    sqlSelect << sqlRet2;
    sqlSelect << sqlRet3;
    sqlSelect << sqlRet4;

    for(auto &line : sqlRet0){
        DebugL << "vector<string> user_id:" << line[0] << ",user_pwd:"<<  line[1];
    }
    for(auto &line : sqlRet1){
        DebugL << "list<string> user_id:" << line.front() << ",user_pwd:"<<  line.back();
    }
    for(auto &line : sqlRet2){
        DebugL << "deque<string> user_id:" << line[0] << ",user_pwd:"<<  line[1];
    }

    for(auto &line : sqlRet3){
        DebugL << "map<string,string> user_id:" << line["user_id"] << ",user_pwd:"<<  line["user_pwd"];
    }

    for(auto &line : sqlRet4){
        DebugL << "unordered_map<string,string> user_id:" << line["user_id"] << ",user_pwd:"<<  line["user_pwd"];
    }

    //异步删除  [AUTO-TRANSLATED:4359ab91]
    // Asynchronous deletion
    SqlWriter insertDel("delete from test_db.test_table where user_name='?';");
    insertDel << "zltoolkit" << endl;

    //注意!  [AUTO-TRANSLATED:acc754b6]
    // Note!
    //如果SqlWriter 的 "<<" 操作符后面紧跟SqlPool::SqlRetType类型，则说明是同步操作并等待结果  [AUTO-TRANSLATED:be2b37a9]
    // If the "<<" operator of SqlWriter is followed by SqlPool::SqlRetType type, it indicates a synchronous operation and waits for the result
    //如果紧跟std::endl ,则是异步操作，在后台线程完成sql操作。  [AUTO-TRANSLATED:982b1ad9]
    // If followed by std::endl, it is an asynchronous operation, which completes the sql operation in the background thread.
#else
    ErrorL << "ENABLE_MYSQL not defined!" << endl;
    return -1;
#endif//ENABLE_MYSQL

    return 0;
}
