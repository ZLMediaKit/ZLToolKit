/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include "Util/CMD.h"
#include "Util/logger.h"
#include "Util/util.h"
#include "Network/TcpClient.h"
#include <signal.h>

using namespace std;
using namespace toolkit;

class TestClient: public TcpClient {
public:
    typedef std::shared_ptr<TestClient> Ptr;
    TestClient() : TcpClient(){}
    ~TestClient(){}
    void connect(const string &strUrl, uint16_t iPort,float fTimeoutSec){
        startConnect(strUrl,iPort,fTimeoutSec);
    }
    void disconnect(){
        shutdown();
    }
    int commit(const string &method,const string &path,const string &host) {
        string strGet = StrPrinter
                << method
                << " "
                << path
                << " HTTP/1.1\r\n"
                << "Host: " << host << "\r\n"
                << "Connection: keep-alive\r\n"
                << "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_12_1) "
                        "AppleWebKit/537.36 (KHTML, like Gecko) "
                        "Chrome/58.0.3029.110 Safari/537.36\r\n"
                << "Accept-Encoding: gzip, deflate, sdch\r\n"
                << "Accept-Language: zh-CN,zh;q=0.8,en;q=0.6\r\n\r\n";
        DebugL << "\r\n" << strGet;
        return SockSender::send(strGet);
    }

protected:
    virtual void onConnect(const SockException &ex) override{
        //连接结果事件
        InfoL << (ex ?  ex.what() : "success");
    }
    virtual void onRecv(const Buffer::Ptr &pBuf) override{
        //接收数据事件
        DebugL << pBuf->data();
    }
    virtual void onFlush() override{
        //发送阻塞后，缓存清空事件
        DebugL;
    }
    virtual void onErr(const SockException &ex) override{
        //断开连接事件，一般是EOF
        WarnL << ex.what();
    }
};

//命令(http)
class CMD_http: public CMD {
public:
    CMD_http(){
        _client.reset(new TestClient);
        _parser.reset(new OptionParser([this](const std::shared_ptr<ostream> &stream,mINI &args){
            //所有选项解析完毕后触发该回调，我们可以在这里做一些全局的操作
            if(hasKey("connect")){
                //发起连接操作
                connect(stream);
                return;
            }
            if(hasKey("commit")){
                commit(stream);
                return;
            }
        }));

        (*_parser) << Option('T', "type", Option::ArgRequired, nullptr, true, "应用程序模式，0：传统模式，1：shell模式", nullptr);

        (*_parser) << Option('s',/*该选项简称，如果是\x00则说明无简称*/
                             "server",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             "www.baidu.com:80",/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "tcp服务器地址，以冒号分隔端口号",/*该选项说明文字*/
                             [this](const std::shared_ptr<ostream> &stream, const string &arg){/*解析到该选项的回调*/
                                 if(arg.find(":") == string::npos){
                                     //中断后续选项的解析以及解析完毕回调等操作
                                     throw std::runtime_error("\t地址必须指明端口号.");
                                 }
                                 //如果返回false则忽略后续选项的解析
                                 return true;
                             });

        (*_parser) << Option('d', "disconnect", Option::ArgNone, nullptr ,false, "是否断开连接",
                             [this](const std::shared_ptr<ostream> &stream, const string &arg){
                                 //断开连接操作，所以后续的参数我们都不解析了
                                 disconnect(stream);
                                 return false;
                             });

        (*_parser) << Option('c', "connect", Option::ArgNone, nullptr, false, "发起tcp connect操作", nullptr);
        (*_parser) << Option('t', "time_out", Option::ArgRequired, "3",false, "连接超时间", nullptr);
        (*_parser) << Option('m', "method", Option::ArgRequired, "GET",false, "HTTP方法,譬如GET、POST", nullptr);
        (*_parser) << Option('p', "path", Option::ArgRequired, "/index.html",false, "HTTP url路径", nullptr);
        (*_parser) << Option('C', "commit", Option::ArgNone, nullptr, false, "提交HTTP请求", nullptr);



    }

    ~CMD_http() {}

    const char *description() const override {
        return "http测试客户端";
    }

private:
    void connect(const std::shared_ptr<ostream> &stream){
        (*stream) << "connect操作" << endl;
        _client->connect(splitedVal("server")[0],splitedVal("server")[1],(*this)["time_out"]);
    }
    void disconnect(const std::shared_ptr<ostream> &stream){
        (*stream) << "disconnect操作" << endl;
        _client->disconnect();
    }
    void commit(const std::shared_ptr<ostream> &stream){
        (*stream) << "commit操作" << endl;
        _client->commit((*this)["method"],(*this)["path"],(*this)["server"]);
    }

private:
    TestClient::Ptr _client;
};



int main(int argc,char *argv[]){
    REGIST_CMD(http);
    signal(SIGINT,[](int ){
        exit(0);
    });
    try{
        CMD_DO("http",argc,argv);
    }catch (std::exception &ex){
        cout << ex.what() << endl;
        return 0;
    }
    if(GET_CMD("http")["type"] == 0){
        cout << "传统模式，已退出程序，请尝试shell模式" << endl;
        return 0;
    }
    GET_CMD("http").delOption("type");
    //初始化环境
    Logger::Instance().add(std::shared_ptr<ConsoleChannel>(new ConsoleChannel()));
    Logger::Instance().setWriter(std::shared_ptr<LogWriter>(new AsyncLogWriter()));

    cout << "> 欢迎进入命令模式，你可以输入\"help\"命令获取帮助" << endl;
    string cmd_line;
    while(cin.good()){
        try{
            cout << "> ";
            getline(cin,cmd_line);
            CMDRegister::Instance()(cmd_line);
        }catch (ExitException &ex){
            break;
        }catch (std::exception &ex){
            cout << ex.what() << endl;
        }
    }
    return 0;
}