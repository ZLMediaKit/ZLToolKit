#include "sockutil.h"
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <assert.h>
#include <netdb.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "Util/logger.h"
using namespace std;
using namespace ZL::Util;

namespace ZL {
namespace Network {

#define LOG_ERROR TRACE

inline const char* errorMsg(int code) {
	return strerror(code);
}
int SockUtil::setNoSigpipe(int sd) {
	int set = 1, ret = 1;
#if defined (__APPLE__)
	ret= setsockopt(sd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
#endif
	return ret;
}

int SockUtil::setNoBlocked(int sock, bool noblock) {
	unsigned long ul = noblock;
	int result = ioctl(sock, FIONBIO, &ul); //设置为非阻塞模式
	if (result == -1) {
		TraceL << "设置非阻塞失败！";
	}
	return result;
}

int SockUtil::connect(const std::string &host, uint16_t port) {
	struct addrinfo *answer = nullptr;
	int ret = getaddrinfo(host.c_str(), NULL, NULL, &answer);
	if (ret < 0 || !answer) {
		WarnL << "域名解析失败：" << host;
		return -1;
	}
	int sock = socket(answer->ai_family, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		freeaddrinfo(answer);
		WarnL << "创建套接字失败:" << host;
		return -1;
	}
	setNoSigpipe(sock);
	setNoBlocked(sock);
	setNoDelay(sock, true);

	int16_t n_port = htons(port);
	memcpy(answer->ai_addr->sa_data, &n_port, 2);
	ret = ::connect(sock, answer->ai_addr, sizeof(struct sockaddr));
	freeaddrinfo(answer);
	if (ret < 0 && errno != EINPROGRESS) {
		WarnL << "连接主机失败：" << host << " " << port << " " << strerror(errno);
		return -1;
	}
	return sock;
}

int SockUtil::setNoDelay(int sockFd, bool on) {
	int opt = on ? 1 : 0;
	int ret = setsockopt(sockFd, IPPROTO_TCP,
	TCP_NODELAY, &opt, static_cast<socklen_t>(sizeof(opt)));
	return ret;
}

int SockUtil::setReuseable(int sockFd, bool on) {
	int opt = on ? 1 : 0;
	return setsockopt(sockFd, SOL_SOCKET,
	SO_REUSEADDR, &opt, static_cast<socklen_t>(sizeof(opt)));
}

int SockUtil::setKeepAlive(int sockFd, bool on) {
	int opt = on ? 1 : 0;
	return setsockopt(sockFd, SOL_SOCKET,
	SO_KEEPALIVE, &opt, static_cast<socklen_t>(sizeof(opt)));
}

int SockUtil::listen(const uint16_t port, const char* localIp, int backLog) {
	int sockfd = -1;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		WarnL << "创建套接字失败:" << strerror(errno);
		return -1;
	}
	setReuseable(sockfd, true);
	setNoBlocked(sockfd);
	//设置监听参数
	struct sockaddr_in servaddr;
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = inet_addr(localIp);
	bzero(&(servaddr.sin_zero), sizeof servaddr.sin_zero);
	//绑定监听
	if (bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) {
		WarnL << "绑定套接字失败:" << strerror(errno);
		close(sockfd);
		return -1;
	}
	//开始监听
	if (::listen(sockfd, backLog) == -1) {
		WarnL << "开始监听失败:" << strerror(errno);
		close(sockfd);
		return -1;
	}

	return sockfd;
}

int SockUtil::getSockError(int sockFd) {
	int opt;
	socklen_t optLen = static_cast<socklen_t>(sizeof(opt));

	if (getsockopt(sockFd, SOL_SOCKET, SO_ERROR, &opt, &optLen) < 0) {
		int err = errno;
		return err;
	} else {
		return opt;
	}
}

string SockUtil::get_local_ip(int fd) {
	struct sockaddr addr;
	struct sockaddr_in* addr_v4;
	socklen_t addr_len = sizeof(addr);
	//获取local ip and port
	memset(&addr, 0, sizeof(addr));
	if (0 == getsockname(fd, &addr, &addr_len)) {
		if (addr.sa_family == AF_INET) {
			addr_v4 = (sockaddr_in*) &addr;
			return string(inet_ntoa(addr_v4->sin_addr));
		}
	}
	return "0.0.0.0";
}

uint16_t SockUtil::get_local_port(int fd) {
	struct sockaddr addr;
	struct sockaddr_in* addr_v4;
	socklen_t addr_len = sizeof(addr);
	//获取remote ip and port
	if (0 == getsockname(fd, &addr, &addr_len)) {
		if (addr.sa_family == AF_INET) {
			addr_v4 = (sockaddr_in*) &addr;
			return ntohs(addr_v4->sin_port);
		}
	}
	return 0;
}

string SockUtil::get_peer_ip(int fd) {
	struct sockaddr addr;
	struct sockaddr_in* addr_v4;
	socklen_t addr_len = sizeof(addr);
	//获取remote ip and port
	if (0 == getpeername(fd, &addr, &addr_len)) {
		if (addr.sa_family == AF_INET) {
			addr_v4 = (sockaddr_in*) &addr;
			return string(inet_ntoa(addr_v4->sin_addr));
		}
	}
	return "0.0.0.0";
}

int SockUtil::bindUdpSock(const uint16_t port, const char* localIp) {
	int sockfd = -1;
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		WarnL << "创建套接字失败:" << strerror(errno);
		return -1;
	}
	setReuseable(sockfd, true);
	setNoBlocked(sockfd);
//设置监听参数
	struct sockaddr_in servaddr;
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = inet_addr(localIp);
	bzero(&(servaddr.sin_zero), sizeof servaddr.sin_zero);
//绑定监听
	if (bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) {
		WarnL << "绑定套接字失败:" << strerror(errno);
		close(sockfd);
		return -1;
	}
	return sockfd;
}

uint16_t SockUtil::get_peer_port(int fd) {
	struct sockaddr addr;
	struct sockaddr_in* addr_v4;
	socklen_t addr_len = sizeof(addr);
	//获取remote ip and port
	if (0 == getpeername(fd, &addr, &addr_len)) {
		if (addr.sa_family == AF_INET) {
			addr_v4 = (sockaddr_in*) &addr;
			return ntohs(addr_v4->sin_port);
		}
	}
	return 0;
}

}  // namespace Network
}  // namespace ZL

