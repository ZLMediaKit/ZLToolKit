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
#include <iostream>
#include "Util/logger.h"
#include "Util/util.h"
#if defined(ENABLE_OPENSSL)
#include "Util/SSLBox.h"
#endif
using namespace std;
using namespace ZL::Util;

int main(int argc,char *argv[]) {
	//初始化设置日志
	Logger::Instance().add(std::make_shared<ConsoleChannel> ("stdout", LTrace));
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

#if defined(ENABLE_OPENSSL)
	//请把证书"test_ssl.pem"放置在本程序同目录下
	try {
		//加载证书，该证书必须为pem格式，里面包含了公钥和私钥
		SSL_Initor::Instance().loadServerPem((exePath() + ".pem").data());
		//定义客户端和服务端
		SSL_Box client(false), server(true);

		//设置客户端解密输出回调
		client.setOnDecData([&](const char *data, uint32_t len) {
			string str(data, len);
			//打印来自服务端数据解密后的明文
			InfoL << "client recv:" << str;
		});

		//设置客户端加密输出回调
		client.setOnEncData([&](const char *data, uint32_t len) {
			//把客户端加密后的密文发送给服务端
			server.onRecv(data, len);
		});

		//设置服务端解密输出回调
		server.setOnDecData([&](const char *data, uint32_t len) {
			//打印来自客户端数据解密后的明文
			string str(data, len);
			InfoL << "server recv:" << str;
			//把数据回显给客户端
			server.onSend(data, len);
		});

		//设置服务端加密输出回调
		server.setOnEncData([&](const char *data, uint32_t len) {
			//把加密的回显信息回复给客户端;
			client.onRecv(data, len);
		});

		InfoL << "请输入字符开始测试,输入quit停止测试:" << endl;

		string input;
		while (true) {
			std::cin >> input;
			if (input == "quit") {
				break;
			}
			//把明文数据输入给客户端
			client.onSend(input.data(), input.size());
		}

	}catch(...){
		DebugL << "请把证书\"test_ssl.pem\"放置在：" << exeDir() << endl;
	}

#else
	FatalL << "ENABLE_OPENSSL 宏未打开";
#endif //ENABLE_OPENSSL


	//程序退出
	Logger::Destory();
	return 0;
}
