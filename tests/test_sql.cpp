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
#include <iostream>
#include "Util/logger.h"
#if defined(ENABLE_MYSQL)
#include "Util/SqlPool.h"
#endif
using namespace std;
using namespace toolkit;

int main() {
	//初始化日志
	Logger::Instance().add(std::make_shared<ConsoleChannel> ());

#if defined(ENABLE_MYSQL)
	/*
	 * 测试方法:
	 * 请按照实际数据库情况修改源码然后编译执行测试
	 */
	string sql_ip = "127.0.0.1";
	unsigned short sql_port = 3306;
	string user = "root";
	string password = "111111";
	string character = "utf8mb4";

#if defined(SUPPORT_DYNAMIC_TEMPLATE)
    //初始化数据
	SqlPool::Instance().Init(sql_ip,sql_port,"",user,password/*,character*/);
#else
    //由于需要编译器对可变参数模板的支持，所以gcc5.0以下一般都不支持，否则编译报错
    ErrorL << "your compiler does not support variable parameter templates!" << endl;
    return -1;
#endif //defined(SUPPORT_DYNAMIC_TEMPLATE)

    //建议数据库连接池大小设置跟CPU个数一致(大一点为佳)
	SqlPool::Instance().setSize(3 + thread::hardware_concurrency());

	vector<vector<string> > sqlRet;
	SqlWriter("create database test_db;", false) << sqlRet;
	SqlWriter("create table test_db.test_table(user_name  varchar(128),user_id int auto_increment primary key,user_pwd varchar(128));", false) << sqlRet;

	//同步插入
	SqlWriter insertSql("insert into test_db.test_table(user_name,user_pwd) values('?','?');");
	insertSql<< "zltoolkit" << "123456" << sqlRet;
	//我们可以知道插入了几条数据，并且可以获取新插入(第一条)数据的rowID
	DebugL << "AffectedRows:" << insertSql.getAffectedRows() << ",RowID:" << insertSql.getRowID();

	//同步查询
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

	//异步删除
	SqlWriter insertDel("delete from test_db.test_table where user_name='?';");
	insertDel << "zltoolkit" << endl;

	//注意!
	//如果SqlWriter 的 "<<" 操作符后面紧跟SqlPool::SqlRetType类型，则说明是同步操作并等待结果
	//如果紧跟std::endl ,则是异步操作，在后台线程完成sql操作。
#else
	ErrorL << "ENABLE_MYSQL not defined!" << endl;
    return -1;
#endif//ENABLE_MYSQL

	return 0;
}
