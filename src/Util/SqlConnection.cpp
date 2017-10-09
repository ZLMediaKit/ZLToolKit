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
#if defined(ENABLE_MYSQL)
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
