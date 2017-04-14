/*
 * SqlConnection.cpp
 *
 *  Created on: 2015年10月29日
 *      Author: root
 */
#ifdef ENABLE_MYSQL
#include "SqlConnection.h"
#include <stdexcept>
namespace ZL {
namespace Util {

SqlConnection::SqlConnection(const string& url, unsigned short port,
		const string& dbname, const string& username, const string& password,const string &character) {
	mysql_init(&sql);
	unsigned int timeout = 3;
	mysql_options(&sql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
	if (!mysql_real_connect(&sql, url.c_str(), username.c_str(),
			password.c_str(), dbname.c_str(), port, NULL, 0)) {
		mysql_close(&sql);
		throw runtime_error(string("mysql_real_connect:")+mysql_error(&sql));
	}
	my_bool reconnect = 1;
	mysql_options(&sql, MYSQL_OPT_RECONNECT, &reconnect);
	mysql_set_character_set(&sql, character.data());
}

SqlConnection::~SqlConnection() {
	mysql_close(&sql);
}

} /* namespace mysql */
} /* namespace im */

#endif //ENABLE_MYSQL
