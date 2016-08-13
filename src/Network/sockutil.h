#pragma once

#include <string>
#include <stdint.h>

using namespace std;

namespace ZL {
namespace Network {

class SockUtil {
public:
	static int connect(const std::string &host, uint16_t port);
	static int listen(const uint16_t port, const char *localIp = "0.0.0.0",
			int backLog = 1024);
	static int bindUdpSock(const uint16_t port, const char *localIp = "0.0.0.0");
	static int setNoDelay(int sockFd, bool on);
	static int setNoSigpipe(int sock);
	static int setNoBlocked(int sock, bool noblock = true);

	static int setReuseable(int sockFd, bool on);
	static int setKeepAlive(int sockFd, bool on);

	static int getSockError(int sockFd);

	static string get_local_ip(int fd);
	static uint16_t get_local_port(int fd);
	static string get_peer_ip(int fd);
	static uint16_t get_peer_port(int fd);
};

}  // namespace Network

}  // namespace ZL

