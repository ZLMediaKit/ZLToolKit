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

#if defined (__APPLE__)
#include <ifaddrs.h>
#endif

#include <arpa/inet.h>
#include "Util/logger.h"
using namespace std;
using namespace ZL::Util;


namespace ZL {
namespace Network {

int SockUtil::setCloseWait(int sockFd, int second) {
	linger m_sLinger;
	//在调用closesocket()时还有数据未发送完，允许等待
	// 若m_sLinger.l_onoff=0;则调用closesocket()后强制关闭
	m_sLinger.l_onoff = (second > 0);
	m_sLinger.l_linger = second; //设置等待时间为x秒
	int ret = setsockopt(sockFd, SOL_SOCKET, SO_LINGER,
			(const char*) &m_sLinger, sizeof(linger));
	if (ret == -1) {
		TraceL << "设置 SO_LINGER 失败！";
	}
	return ret;
}
int SockUtil::setNoDelay(int sockFd, bool on) {
	int opt = on ? 1 : 0;
	int ret = setsockopt(sockFd, IPPROTO_TCP, TCP_NODELAY, &opt,
			static_cast<socklen_t>(sizeof(opt)));
	if (ret == -1) {
		TraceL << "设置 NoDelay 失败！";
	}
	return ret;
}

int SockUtil::setReuseable(int sockFd, bool on) {
	int opt = on ? 1 : 0;
	int ret = setsockopt(sockFd, SOL_SOCKET,
	SO_REUSEADDR, &opt, static_cast<socklen_t>(sizeof(opt)));
	if (ret == -1) {
		TraceL << "设置 SO_REUSEADDR 失败！";
	}
	return ret;
}
int SockUtil::setBroadcast(int sockFd, bool on) {
	int opt = on ? 1 : 0;
	int ret = setsockopt(sockFd, SOL_SOCKET, SO_BROADCAST, &opt,
			static_cast<socklen_t>(sizeof(opt)));
	if (ret == -1) {
		TraceL << "设置 SO_BROADCAST 失败！";
	}
	return ret;
}

int SockUtil::setKeepAlive(int sockFd, bool on) {
	int opt = on ? 1 : 0;
	int ret = setsockopt(sockFd, SOL_SOCKET, SO_KEEPALIVE, &opt,
			static_cast<socklen_t>(sizeof(opt)));
	if (ret == -1) {
		TraceL << "设置 SO_KEEPALIVE 失败！";
	}
	return ret;
}

int SockUtil::setNoSigpipe(int sd) {
	int set = 1, ret = 1;
#ifdef SO_NOSIGPIPE
	ret= setsockopt(sd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
	if (ret == -1) {
		TraceL << "设置 SO_NOSIGPIPE 失败！";
	}
#endif
	return ret;
}

int SockUtil::setNoBlocked(int sock, bool noblock) {
	unsigned long ul = noblock;
	int ret = ioctl(sock, FIONBIO, &ul); //设置为非阻塞模式
	if (ret == -1) {
		TraceL << "设置非阻塞失败！";
	}
    
	return ret;
}

int SockUtil::setRecvBuf(int sock, int size) {
	int ret = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
	if (ret == -1) {
		TraceL << "设置接收缓冲区失败！";
	}
	return ret;
}
int SockUtil::setSendBuf(int sock, int size) {
	int ret = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
	if (ret == -1) {
		TraceL << "设置发送缓冲区失败！";
	}
	return ret;
}

int SockUtil::connect(const char *host, uint16_t port) {
    
#if defined (__APPLE__)
	
    struct addrinfo *answer=nullptr;
    int ret = getaddrinfo(host, NULL, NULL, &answer);
    if (ret < 0 || !answer) {
        WarnL << "域名解析失败：" << host;
        return -1;
    }
    int sockfd= socket(answer->ai_family, SOCK_STREAM , IPPROTO_TCP);
    if (sockfd < 0) {
        freeaddrinfo(answer);
        WarnL << "创建套接字失败：" << host;
        return -1;
    }
    setNoSigpipe(sockfd);
    setNoBlocked(sockfd);
    setNoDelay(sockfd);
    setSendBuf(sockfd);
    setRecvBuf(sockfd);
    setCloseWait(sockfd);
    
    int16_t n_port=htons(port);
    memcpy(answer->ai_addr->sa_data, &n_port, 2);
    ret = ::connect(sockfd, answer->ai_addr, sizeof(struct sockaddr));
    freeaddrinfo(answer);
    if(ret < 0 && errno!=EINPROGRESS){
        WarnL << "连接主机失败：" << host << " " << port << " " << strerror(errno);
        close(sockfd);
        return -1;
    }
    return sockfd;
    
#else
    
    struct hostent *hp = gethostbyname(host);
    if (hp == NULL) {
        WarnL << "域名解析失败：" << host;
        return false;
    }
    struct in_addr *addr = (in_addr*) hp->h_addr_list[0];
    if (addr == NULL) {
        //freehostent(hp);
        WarnL << "域名解析失败：" << host;
        return false;
    }
    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr = *addr;
    bzero(&(servaddr.sin_zero), sizeof servaddr.sin_zero);
    //freehostent(hp);
    
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        WarnL << "创建套接字失败:" << host;
        return -1;
    }
    
    setNoSigpipe(sockfd);
    setNoBlocked(sockfd);
    setNoDelay(sockfd);
    setSendBuf(sockfd);
    setRecvBuf(sockfd);
    setCloseWait(sockfd);
    
    int ret = ::connect(sockfd, (struct sockaddr *) &servaddr,
                        sizeof(struct sockaddr));
    if (ret < 0 && errno != EINPROGRESS) {
        WarnL << "连接主机失败：" << host << " " << port << " " << strerror(errno);
		close(sockfd);
        return -1;
    }
    return sockfd;
#endif //__APPLE__
}

int SockUtil::listen(const uint16_t port, const char* localIp, int backLog) {
	int sockfd = -1;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		WarnL << "创建套接字失败:" << strerror(errno);
		return -1;
	}

	setReuseable(sockfd);
	setNoBlocked(sockfd);

	//设置监听参数
	struct sockaddr_in servaddr;
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = inet_addr(localIp);
	bzero(&(servaddr.sin_zero), sizeof servaddr.sin_zero);
	//绑定监听
    if (::bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) {
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
#if defined (__APPLE__)
string getIosIp(){
    string address = "127.0.0.1";
    struct ifaddrs *interfaces = NULL;
    struct ifaddrs *temp_addr = NULL;
    int success = 0;
    // retrieve the current interfaces - returns 0 on success
    success = getifaddrs(&interfaces);
    if (success == 0) {
        // Loop through linked list of interfaces
        temp_addr = interfaces;
        while(temp_addr != NULL) {
            if(temp_addr->ifa_addr->sa_family == AF_INET) {
                // Check if interface is en0 which is the wifi connection on the iPhone
                if(strcmp(temp_addr->ifa_name,"en0")==0) {
                    // Get NSString from C String
                    address = inet_ntoa(((struct sockaddr_in *)temp_addr->ifa_addr)->sin_addr);
                }
            }
            temp_addr = temp_addr->ifa_next;
        }
    }
    // Free memory
    freeifaddrs(interfaces);
    return address;
}
#endif //__APPLE_

string SockUtil::get_local_ip() {
#if defined (__APPLE__)
    return getIosIp();
#endif //__APPLE__
	int i = 0;
	int sockfd;
	struct ifconf ifconf;
	char buf[512];
	struct ifreq *ifreq;
	char* ip;
	//初始化ifconf
	ifconf.ifc_len = 512;
	ifconf.ifc_buf = buf;

	if ((sockfd = ::socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		WarnL << "创建套接字失败:" << strerror(errno);
		return "127.0.0.1";
	}
	if (-1 == ioctl(sockfd, SIOCGIFCONF, &ifconf)) {    //获取所有接口信息
		WarnL << "ioctl 失败:" << strerror(errno);
		close(sockfd);
		return "127.0.0.1";
	}
	close(sockfd);
	//接下来一个一个的获取IP地址
	ifreq = (struct ifreq*) buf;

	for (i = (ifconf.ifc_len / sizeof(struct ifreq)); i > 0; i--) {
		ip = inet_ntoa(((struct sockaddr_in*) &(ifreq->ifr_addr))->sin_addr);
		//排除127.0.0.1，继续下一个
		if (strcmp(ip, "127.0.0.1") == 0) {
			ifreq++;
			continue;
		}
		return ip;
	}

	return "127.0.0.1";
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
	setReuseable(sockfd);
	setNoSigpipe(sockfd);
	setNoBlocked(sockfd);
	setSendBuf(sockfd);
	setRecvBuf(sockfd);
	setCloseWait(sockfd);
//设置监听参数
	struct sockaddr_in servaddr;
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = inet_addr(localIp);
	bzero(&(servaddr.sin_zero), sizeof servaddr.sin_zero);
//绑定监听
    if (::bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) {
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
string SockUtil::get_ifr_name(const char *localIp){
	int i = 0;
	int sockfd;
	struct ifconf ifconf;
	char buf[512];
	struct ifreq *ifreq;
	char *ip;
	//初始化ifconf
	ifconf.ifc_len = 512;
	ifconf.ifc_buf = buf;

	if ((sockfd = ::socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		WarnL << "创建套接字失败:" << strerror(errno);
		return "en0";
	}
	if (-1 == ioctl(sockfd, SIOCGIFCONF, &ifconf)) {    //获取所有接口信息
		WarnL << "ioctl 失败:" << strerror(errno);
		close(sockfd);
		return "en0";
	}
	close(sockfd);
	//接下来一个一个的获取IP地址
	ifreq = (struct ifreq*) buf;

	for (i = (ifconf.ifc_len / sizeof(struct ifreq)); i > 0; i--) {
		ip = inet_ntoa(((struct sockaddr_in*) &(ifreq->ifr_addr))->sin_addr);
        //DebugL << ip << " " << ifreq->ifr_name;
		if (strcmp(ip,localIp) != 0) {
			ifreq++;
			continue;
		}
		return ifreq->ifr_name;
	}
	return "en0";
}


string SockUtil::get_ifr_mask(const char* ifrName) {
	int sockFd;
	struct ifreq ifr_mask;
	sockFd = socket( AF_INET, SOCK_STREAM, 0);
	if (sockFd == -1) {
		WarnL << "创建套接字失败:" << strerror(errno);
		return "";
	}
	memset(&ifr_mask, 0, sizeof(ifr_mask));
	strncpy(ifr_mask.ifr_name, ifrName, sizeof(ifr_mask.ifr_name) - 1);
	if ((ioctl(sockFd, SIOCGIFNETMASK, &ifr_mask)) < 0) {
		WarnL << "ioctl 失败:" << strerror(errno);
		close(sockFd);
		return "";
	}
	close(sockFd);
#if defined (__APPLE__)
    return inet_ntoa(((struct sockaddr_in *) &(ifr_mask.ifr_ifru.ifru_broadaddr))->sin_addr);
#else
	return inet_ntoa(((struct sockaddr_in *) &(ifr_mask.ifr_netmask))->sin_addr);
#endif //__APPLE__
}

string SockUtil::get_ifr_brdaddr(const char *ifrName){
	int sockFd;
	struct ifreq ifr_mask;
	sockFd = socket( AF_INET, SOCK_STREAM, 0);
	if (sockFd == -1) {
		WarnL << "创建套接字失败:" << strerror(errno);
		return "";
	}
	memset(&ifr_mask, 0, sizeof(ifr_mask));
	strncpy(ifr_mask.ifr_name, ifrName, sizeof(ifr_mask.ifr_name) - 1);
	if ((ioctl(sockFd, SIOCGIFBRDADDR, &ifr_mask)) < 0) {
		WarnL << "ioctl 失败:" << strerror(errno);
		close(sockFd);
		return "";
	}
	close(sockFd);
	return inet_ntoa(((struct sockaddr_in *) &(ifr_mask.ifr_broadaddr))->sin_addr);
}
#define ip_addr_netcmp(addr1, addr2, mask) (((addr1) & (mask)) == ((addr2) & (mask)))
bool SockUtil::in_same_lan(const char *myIp,const char *dstIp){
	string mask = get_ifr_mask(get_ifr_name(myIp).data());
	return ip_addr_netcmp(inet_addr(myIp),inet_addr(dstIp),inet_addr(mask.data()));
}


static void clearMulticastAllSocketOption(int socket) {
#ifdef IP_MULTICAST_ALL
  // This option is defined in modern versions of Linux to overcome a bug in the Linux kernel's default behavior.
  // When set to 0, it ensures that we receive only packets that were sent to the specified IP multicast address,
  // even if some other process on the same system has joined a different multicast group with the same port number.
  int multicastAll = 0;
  (void)setsockopt(socket, IPPROTO_IP, IP_MULTICAST_ALL, (void*)&multicastAll, sizeof multicastAll);
  // Ignore the call's result.  Should it fail, we'll still receive packets (just perhaps more than intended)
#endif
}

int SockUtil::setMultiTTL(int sockFd, uint8_t ttl) {
	int ret = -1;
#ifdef IP_MULTICAST_TTL
	ret= setsockopt(sockFd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
	if (ret == -1) {
		TraceL << "设置 IP_MULTICAST_TTL 失败！";
	}
#endif
	clearMulticastAllSocketOption(sockFd);
	return ret;
}

int SockUtil::setMultiIF(int sockFd, const char* strLocalIp) {
	int ret = -1;
#ifdef IP_MULTICAST_IF
	struct in_addr addr;
	addr.s_addr = inet_addr(strLocalIp);
	ret = setsockopt(sockFd, IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr));
	if (ret == -1) {
		TraceL << "设置 IP_MULTICAST_IF 失败！";
	}
#endif
	clearMulticastAllSocketOption(sockFd);
	return ret;
}

int SockUtil::setMultiLOOP(int sockFd, bool bAccept) {
	int ret = -1;
#ifdef IP_MULTICAST_LOOP
	uint8_t loop = bAccept;
	ret = setsockopt(sockFd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
	if (ret == -1) {
		TraceL << "设置 IP_MULTICAST_LOOP 失败！";
	}
#endif
	clearMulticastAllSocketOption(sockFd);
	return ret;
}

int SockUtil::joinMultiAddr(int sockFd, const char* strAddr,const char* strLocalIp) {
	int ret = -1;
#ifdef IP_ADD_MEMBERSHIP
	struct ip_mreq imr;
	imr.imr_multiaddr.s_addr = inet_addr(strAddr);
	imr.imr_interface.s_addr = inet_addr(strLocalIp);
	ret = setsockopt(sockFd, IPPROTO_IP, IP_ADD_MEMBERSHIP,  (const char*)&imr, sizeof (struct ip_mreq));
	if (ret == -1) {
		TraceL << "设置 IP_ADD_MEMBERSHIP 失败:" << strerror(errno);
	}
#endif
	clearMulticastAllSocketOption(sockFd);
	return ret;
}

int SockUtil::leaveMultiAddr(int sockFd, const char* strAddr,const char* strLocalIp) {
	int ret = -1;
#ifdef IP_DROP_MEMBERSHIP
	struct ip_mreq imr;
	imr.imr_multiaddr.s_addr = inet_addr(strAddr);
	imr.imr_interface.s_addr = inet_addr(strLocalIp);
	ret = setsockopt(sockFd, IPPROTO_IP, IP_DROP_MEMBERSHIP,  (const char*)&imr, sizeof (struct ip_mreq));
	if (ret == -1) {
		TraceL << "设置 IP_DROP_MEMBERSHIP 失败:" << strerror(errno);;
	}
#endif
	clearMulticastAllSocketOption(sockFd);
	return ret;
}

int SockUtil::joinMultiAddrFilter(int sockFd, const char* strAddr,
		const char* strSrcIp, const char* strLocalIp) {
	int ret = -1;
#ifdef IP_ADD_SOURCE_MEMBERSHIP
	struct ip_mreq_source imr;
#ifdef ANDROID
	imr.imr_multiaddr = inet_addr(strAddr);
	imr.imr_sourceaddr = inet_addr(strSrcIp);
	imr.imr_interface = inet_addr(strLocalIp);
#else
	imr.imr_multiaddr.s_addr = inet_addr(strAddr);
	imr.imr_sourceaddr.s_addr = inet_addr(strSrcIp);
	imr.imr_interface.s_addr =  inet_addr(strLocalIp);
#endif
	ret = setsockopt(sockFd, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP,
			(const char*) &imr, sizeof(struct ip_mreq_source));
	if (ret == -1) {
		TraceL << "设置 IP_ADD_SOURCE_MEMBERSHIP 失败:" << strerror(errno);
	}
#endif
	clearMulticastAllSocketOption(sockFd);
	return ret;
}

int SockUtil::leaveMultiAddrFilter(int sockFd, const char* strAddr,
		const char* strSrcIp, const char* strLocalIp) {
	int ret = -1;
#ifdef IP_DROP_SOURCE_MEMBERSHIP
	struct ip_mreq_source imr;
#ifdef ANDROID
	imr.imr_multiaddr = inet_addr(strAddr);
	imr.imr_sourceaddr = inet_addr(strSrcIp);
	imr.imr_interface = inet_addr(strLocalIp);
#else
	imr.imr_multiaddr.s_addr = inet_addr(strAddr);
	imr.imr_sourceaddr.s_addr = inet_addr(strSrcIp);
	imr.imr_interface.s_addr = inet_addr(strLocalIp);
#endif
	ret = setsockopt(sockFd, IPPROTO_IP, IP_DROP_SOURCE_MEMBERSHIP,
			(const char*) &imr, sizeof(struct ip_mreq_source));
	if (ret == -1) {
		TraceL << "设置 IP_DROP_SOURCE_MEMBERSHIP 失败:" << strerror(errno);
	}
#endif
	clearMulticastAllSocketOption(sockFd);
	return ret;
}

















}  // namespace Network
}  // namespace ZL


