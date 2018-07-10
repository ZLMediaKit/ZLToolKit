//
// Created by rbcheng on 18-7-9.
// Email: rbcheng@qq.com
//

#include <Network/LengthTcpSession.h>
#include <Network/TcpServer.h>
#include <signal.h>

using namespace std;
using namespace ZL::Util;
using namespace ZL::Network;

void packet_buffer(BufferRaw::Ptr& ptr, const string& data) {
    int data_len = data.size();
    char* t_data_len = (char*)malloc(sizeof(unsigned int));
    _Int2Chars(t_data_len, data_len);
    ptr->setCapacity(sizeof(unsigned int) + data_len);
    ptr->append(t_data_len, sizeof(unsigned int));
    ptr->append(data.c_str(), data_len);
}

void test_handle_packet() {
    LengthTcpSession::Ptr session_ptr = make_shared<LengthTcpSession>(WorkThreadPool().Instance().getWorkThread(), make_shared<Socket>());

    BufferRaw::Ptr ptr = std::make_shared<BufferRaw>();
    packet_buffer(ptr, "");

    char* buffer = ptr->data();
    int stamp = _Chars2Int(buffer);
    InfoL << stamp;
    int size = ptr->size();
    int fragment = 16792;
    for (int i = 0; i < size; i += fragment) {
        int temp_fragment = fragment;
//        session_ptr->handle_buffer(buffer, temp_fragment);
        buffer += fragment;
//        ThreadPool::Instance().sync([&]() {
//            session_ptr->handle_buffer(buffer, fragment);
//            buffer += fragment;
//            usleep(1000);
//        });
    }

}

int main() {
    //退出程序事件处理
    signal(SIGINT, [](int){EventPoller::Instance().shutdown();});
    //初始化日志模块
    Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    TcpServer::Ptr server(new TcpServer());
    server->start<LengthTcpSession>(9000);//监听9000端口

    EventPoller::Instance().runLoop();//主线程事件轮询

//    server.reset();//销毁服务器
    //TcpServer 依赖线程池，需要销毁
    WorkThreadPool::Destory();
    EventPoller::Destory();
    Logger::Destory();
    return 0;
}
