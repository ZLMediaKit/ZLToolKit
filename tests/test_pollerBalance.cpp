#include <csignal>
#include <iostream>
#include <vector>
#if defined(__linux__) || defined(__linux)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include "Util/logger.h"
#include "Network/TcpServer.h"
#include "Network/Session.h"

using namespace std;
using namespace toolkit;

class EchoSession : public Session {
public:
    EchoSession(const Socket::Ptr &sock) : Session(sock) {}
    void onRecv(const Buffer::Ptr &buf) override { send(buf); }
    void onError(const SockException &) override {}
    void onManager() override {}
};

int main(int argc, char *argv[]) {
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
    Logger::Instance().setLevel(LWarn);
    int threads = argc > 1 ? atoi(argv[1]) : std::thread::hardware_concurrency();
    int conns = argc > 2 ? atoi(argv[2]) : 4000;
    EventPollerPool::setPoolSize(threads);

    TcpServer::Ptr server(new TcpServer());
    server->start<EchoSession>(9011);

    vector<int> fds;
    for (int i = 0; i < conns; ++i) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(9011);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (connect(fd, (sockaddr *)&addr, sizeof(addr)) == 0) {
            fds.push_back(fd);
        } else {
            close(fd);
        }
    }
    this_thread::sleep_for(chrono::seconds(1));

    size_t total = 0, maxv = 0, minv = SIZE_MAX;
    EventPollerPool::Instance().for_each([&](const TaskExecutor::Ptr &ex) {
        auto p = static_pointer_cast<EventPoller>(ex);
        auto n = p->fdCount();
        total += n;
        maxv = max(maxv, n);
        minv = min(minv, n);
        cout << p->getThreadName() << ": fd=" << n << " pending=" << p->pendingFdCount() << endl;
    });
    cout << "connected=" << fds.size() << " total_fd=" << total << " max=" << maxv << " min=" << minv << endl;
    // 先停服务器再断开客户端, 等待poller处理完关闭事件后退出, 避免退出时的析构竞争
    server.reset();
    for (auto fd : fds) close(fd);
    this_thread::sleep_for(chrono::seconds(1));
    return 0;
}
