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
#include "Util/util.h"
#include "Util/SSLBox.h"
using namespace std;
using namespace toolkit;

int main(int argc,char *argv[]) {
	//初始化设置日志
	Logger::Instance().add(std::make_shared<ConsoleChannel> ());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

	//加载证书，证书包含公钥和私钥
	SSL_Initor::Instance().loadCertificate((exeDir() + "ssl.p12").data());
	SSL_Initor::Instance().trustCertificate((exeDir() + "ssl.p12").data());
    SSL_Initor::Instance().ignoreInvalidCertificate(false);

    //定义客户端和服务端
	SSL_Box client(false), server(true);

	//设置客户端解密输出回调
	client.setOnDecData([&](const Buffer::Ptr &buffer) {
		//打印来自服务端数据解密后的明文
		InfoL << "client recv:" << buffer->toString();
	});

	//设置客户端加密输出回调
	client.setOnEncData([&](const Buffer::Ptr &buffer) {
		//把客户端加密后的密文发送给服务端
		server.onRecv(buffer);
	});

	//设置服务端解密输出回调
	server.setOnDecData([&](const Buffer::Ptr &buffer) {
		//打印来自客户端数据解密后的明文
		InfoL << "server recv:" << buffer->toString();
		//把数据回显给客户端
		server.onSend(buffer);
	});

	//设置服务端加密输出回调
	server.setOnEncData([&](const Buffer::Ptr &buffer) {
		//把加密的回显信息回复给客户端;
		client.onRecv(buffer);
	});

	InfoL << "请输入字符开始测试,输入quit停止测试:" << endl;

	string input;
	while (true) {
		std::cin >> input;
		if (input == "quit") {
			break;
		}
		//把明文数据输入给客户端
		client.onSend(std::make_shared<BufferString>(std::move(input)));
	}
	return 0;
}
