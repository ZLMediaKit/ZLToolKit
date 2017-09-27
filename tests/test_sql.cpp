
#include <iostream>
#ifdef __clang__
#undef __GNUC__
#endif //__clang__
#include "Util/logger.h"
#if defined(ENABLE_MYSQL)
#include "Util/SqlPool.h"
#endif
using namespace std;
using namespace ZL::Util;

int main() {
	//初始化日志
	Logger::Instance().add(std::make_shared<ConsoleChannel> ("stdout", LTrace));

#if defined(ENABLE_MYSQL)
	/*
	 * 测试方法:
	 * 请按照实际数据库情况修改源码然后编译执行测试
	 */
	string sql_ip = "192.168.0.124";
	unsigned short sql_port = 3306;
	string dbname = "jzandb";
	string user = "root";
	string password = "111111";
	string character = "utf8mb4";

#if !defined(__GNUC__) || (__GNUC__ >= 5)
    //初始化数据
	SqlPool::Instance().Init(sql_ip,sql_port,dbname,user,password/*,character*/);
#else
    //由于需要编译器对可变参数模板的支持，所以gcc5.0以下一般都不支持，否则编译报错
    FatalL << "your compiler does not support variable parameter templates!" << endl;
    return -1;
#endif //(!defined(__GNUC__)) || (__GNUC__ >= 5)

    //建议数据库连接池大小设置跟CPU个数一致(大一点为佳)
	SqlPool::Instance().reSize(3 + thread::hardware_concurrency());

	//sql查询结果保存对象
	SqlPool::SqlRetType sqlRet;

	//同步插入
	SqlWriter insertSql("insert into jzan_user(user_name,user_pwd) values('?','?');" , false);
	insertSql<< "zltoolkit" << "123456" << sqlRet;
	//我们可以知道插入了几条数据，并且可以获取新插入(第一条)数据的rowID
	DebugL << insertSql.getAffectedRows() << "," << insertSql.getRowID();

	//同步查询
	SqlWriter sqlSelect("select user_id , user_pwd from jzan_user where user_name='?' limit 1;") ;
	sqlSelect << "zltoolkit" << sqlRet;
	for(auto &line : sqlRet){
		DebugL << "user_id:" << line[0] << ",user_pwd:"<<  line[1];
	}

	//异步删除
	SqlWriter insertDel("delete from jzan_user where user_name='?';");
	insertDel << "zltoolkit" << endl;

	//注意!
	//如果SqlWriter 的 "<<" 操作符后面紧跟SqlPool::SqlRetType类型，则说明是同步操作并等待结果
	//如果紧跟std::endl ,则是异步操作，在后台线程完成sql操作。
#else
	FatalL << "ENABLE_MYSQL not defined!" << endl;
    return -1;
#endif//ENABLE_MYSQL

	//程序退出...
#if defined(ENABLE_MYSQL)
	sleep(1);
	SqlPool::Destory();
#endif//ENABLE_MYSQL
	Logger::Destory();
	return 0;
}
