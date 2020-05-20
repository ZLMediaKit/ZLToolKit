/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <mutex>
#include <string>
#include <unordered_map>
#include "sockutil.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/uv_errno.h"
#include "Util/onceToken.h"

#if defined (__APPLE__)
#include <ifaddrs.h>
#endif

using namespace std;

namespace toolkit {

#if defined(_WIN32)
    static onceToken g_token([]() {
        WORD wVersionRequested = MAKEWORD(2, 2);
        WSADATA wsaData;
        WSAStartup(wVersionRequested, &wsaData);
    }, []() {
        WSACleanup();
    });
    int ioctl(int fd, long cmd, u_long *ptr) {
        return ioctlsocket(fd, cmd, ptr);
    }
    int close(int fd) {
        return closesocket(fd);
    }
#endif // defined(_WIN32)

string SockUtil::inet_ntoa(struct in_addr &addr) {
    char buf[20];
    unsigned char *p = (unsigned char *) &(addr);
    sprintf(buf, "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);
    return buf;
}

int SockUtil::setCloseWait(int sockFd, int second) {
    linger m_sLinger;
    //在调用closesocket()时还有数据未发送完，允许等待
    // 若m_sLinger.l_onoff=0;则调用closesocket()后强制关闭
    m_sLinger.l_onoff = (second > 0);
    m_sLinger.l_linger = second; //设置等待时间为x秒
    int ret = setsockopt(sockFd, SOL_SOCKET, SO_LINGER,
            (char*) &m_sLinger, sizeof(linger));
    if (ret == -1) {
        TraceL << "设置 SO_LINGER 失败!";
    }
    return ret;
}
int SockUtil::setNoDelay(int sockFd, bool on) {
    int opt = on ? 1 : 0;
    int ret = setsockopt(sockFd, IPPROTO_TCP, TCP_NODELAY,(char *)&opt,static_cast<socklen_t>(sizeof(opt)));
    if (ret == -1) {
        TraceL << "设置 NoDelay 失败!";
    }
    return ret;
}

int SockUtil::setReuseable(int sockFd, bool on) {
    int opt = on ? 1 : 0;
    int ret = setsockopt(sockFd, SOL_SOCKET,
    SO_REUSEADDR, (char *)&opt, static_cast<socklen_t>(sizeof(opt)));
    if (ret == -1) {
        TraceL << "设置 SO_REUSEADDR 失败!";
    }
    return ret;
}
int SockUtil::setBroadcast(int sockFd, bool on) {
    int opt = on ? 1 : 0;
    int ret = setsockopt(sockFd, SOL_SOCKET, SO_BROADCAST, (char *)&opt,static_cast<socklen_t>(sizeof(opt)));
    if (ret == -1) {
        TraceL << "设置 SO_BROADCAST 失败!";
    }
    return ret;
}

int SockUtil::setKeepAlive(int sockFd, bool on) {
    int opt = on ? 1 : 0;
    int ret = setsockopt(sockFd, SOL_SOCKET, SO_KEEPALIVE, (char *)&opt,static_cast<socklen_t>(sizeof(opt)));
    if (ret == -1) {
        TraceL << "设置 SO_KEEPALIVE 失败!";
    }
    return ret;
}

int SockUtil::setNoSigpipe(int sd) {
    int set = 1, ret = 1;
#if defined(SO_NOSIGPIPE)
    ret= setsockopt(sd, SOL_SOCKET, SO_NOSIGPIPE, (char*)&set, sizeof(int));
    if (ret == -1) {
        TraceL << "设置 SO_NOSIGPIPE 失败!";
    }
#endif
    return ret;
}

int SockUtil::setNoBlocked(int sock, bool noblock) {
#if defined(_WIN32)
    unsigned long ul = noblock;
#else
    int ul = noblock;
#endif //defined(_WIN32)
    int ret = ioctl(sock, FIONBIO, &ul); //设置为非阻塞模式
    if (ret == -1) {
        TraceL << "设置非阻塞失败!";
    }
    
    return ret;
}

int SockUtil::setRecvBuf(int sock, int size) {
    int ret = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&size, sizeof(size));
    if (ret == -1) {
        TraceL << "设置接收缓冲区失败!";
    }
    return ret;
}
int SockUtil::setSendBuf(int sock, int size) {
    int ret = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&size, sizeof(size));
    if (ret == -1) {
        TraceL << "设置发送缓冲区失败!";
    }
    return ret;
}



class DnsCache {
public:

    static DnsCache &Instance(){
        static DnsCache instance;
        return instance;
    }
    bool getDomainIP(const char *host,sockaddr &addr,int expireSec = 60){
        DnsItem item;
        auto flag = getCacheDomainIP(host,item,expireSec);
        if(!flag){
            flag = getSystemDomainIP(host,item._addr);
            if(flag){
                setCacheDomainIP(host,item);
            }
        }
        if(flag){
            addr = item._addr;
        }
        return flag;
    }
private:
    DnsCache(){}
    ~DnsCache(){}

    class DnsItem{
    public:
        sockaddr _addr;
        time_t _create_time;
    };

    bool getCacheDomainIP(const char *host,DnsItem &item,int expireSec){
        lock_guard<mutex> lck(_mtx);
        auto it = _mapDns.find(host);
        if(it == _mapDns.end()){
            //没有记录
            return false;
        }
        if(it->second._create_time + expireSec < time(NULL)){
            //已过期
            _mapDns.erase(it);
            return false;
        }
        item = it->second;
        return true;
    }
    void setCacheDomainIP(const char *host,DnsItem &item){
        lock_guard<mutex> lck(_mtx);
        item._create_time = time(NULL);
        _mapDns[host] = item;
    }

    bool getSystemDomainIP(const char *host , sockaddr &item ){
        struct addrinfo *answer=nullptr;
        //阻塞式dns解析，可能被打断
        int ret = -1;
        do{
            ret = getaddrinfo(host, NULL, NULL, &answer);
        }while(ret == -1 && get_uv_error(true) == UV_EINTR) ;

        if (!answer) {
            WarnL << "域名解析失败:" << host;
            return false;
        }
        item = *(answer->ai_addr);
        freeaddrinfo(answer);
        return true;
    }
private:
    mutex _mtx;
    unordered_map<string,DnsItem> _mapDns;
};


bool SockUtil::getDomainIP(const char *host,uint16_t port,struct sockaddr &addr){
    bool flag = DnsCache::Instance().getDomainIP(host,addr);
    if(flag){
        ((sockaddr_in *)&addr)->sin_port = htons(port);
    }
    return flag;
}

int SockUtil::connect(const char *host, uint16_t port,bool bAsync,const char *localIp ,uint16_t localPort) {
    sockaddr addr;
    if(!DnsCache::Instance().getDomainIP(host,addr)){
        //dns解析失败
        return -1;
    }
    //设置端口号
    ((sockaddr_in *)&addr)->sin_port = htons(port);

    int sockfd= socket(addr.sa_family, SOCK_STREAM , IPPROTO_TCP);
    if (sockfd < 0) {
        WarnL << "创建套接字失败:" << host;
        return -1;
    }

    setReuseable(sockfd);
    setNoSigpipe(sockfd);
    setNoBlocked(sockfd, bAsync);
    setNoDelay(sockfd);
    setSendBuf(sockfd);
    setRecvBuf(sockfd);
    setCloseWait(sockfd);

    if(bindSock(sockfd,localIp,localPort) == -1){
        close(sockfd);
        return -1;
    }

    if (::connect(sockfd, &addr, sizeof(struct sockaddr)) == 0) {
        //同步连接成功
        return sockfd;
    }
    if (bAsync &&  get_uv_error(true) == UV_EAGAIN) {
        //异步连接成功
        return sockfd;
    }
    WarnL << "连接主机失败:" << host << " " << port << " " << get_uv_errmsg(true);
    close(sockfd);
    return -1;
}

int SockUtil::listen(const uint16_t port, const char* localIp, int backLog) {
    int sockfd = -1;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        WarnL << "创建套接字失败:" << get_uv_errmsg(true);
        return -1;
    }

    setReuseable(sockfd);
    setNoBlocked(sockfd);

    if(bindSock(sockfd,localIp,port) == -1){
        close(sockfd);
        return -1;
    }

    //开始监听
    if (::listen(sockfd, backLog) == -1) {
        WarnL << "开始监听失败:" << get_uv_errmsg(true);
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int SockUtil::getSockError(int sockFd) {
    int opt;
    socklen_t optLen = static_cast<socklen_t>(sizeof(opt));

    if (getsockopt(sockFd, SOL_SOCKET, SO_ERROR, (char *)&opt, &optLen) < 0) {
        int err = get_uv_error(true);
        return err;
    } else {
        return uv_translate_posix_error(opt);
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
            return SockUtil::inet_ntoa(addr_v4->sin_addr);
        }
    }
    return "";
}



#if defined(__APPLE__)
template<typename FUN>
void for_each_netAdapter_apple(FUN &&fun){ //type: struct ifaddrs *
    struct ifaddrs *interfaces = NULL;
    struct ifaddrs *adapter = NULL;
    if (getifaddrs(&interfaces) == 0) {
        adapter = interfaces;
        while(adapter) {
            if(adapter->ifa_addr->sa_family == AF_INET) {
                if(fun(adapter)){
                    break;
                }
            }
            adapter = adapter->ifa_next;
        }
        freeifaddrs(interfaces);
    }
}
#endif //defined(__APPLE__)

#if defined(_WIN32)
template<typename FUN>
void for_each_netAdapter_win32(FUN && fun) { //type: PIP_ADAPTER_INFO
    unsigned long nSize = sizeof(IP_ADAPTER_INFO);
    PIP_ADAPTER_INFO adapterList = (PIP_ADAPTER_INFO)new char[nSize];
    int nRet = GetAdaptersInfo(adapterList, &nSize);
    if (ERROR_BUFFER_OVERFLOW == nRet) {
        delete[] adapterList;
        adapterList = (PIP_ADAPTER_INFO)new char[nSize];
        nRet = GetAdaptersInfo(adapterList, &nSize);
    }
    auto adapterPtr = adapterList;
    while (adapterPtr && ERROR_SUCCESS == nRet) {
        if (fun(adapterPtr)) {
            break;
        }
        adapterPtr = adapterPtr->Next;
    }
    //释放内存空间
    delete[] adapterList;
}
#endif //defined(_WIN32)

#if !defined(_WIN32) && !defined(__APPLE__)
template<typename FUN>
void for_each_netAdapter_posix(FUN &&fun){ //type: struct ifreq *
    struct ifconf ifconf;
    char buf[1024 * 10];
    //初始化ifconf
    ifconf.ifc_len = sizeof(buf);
    ifconf.ifc_buf = buf;
    int sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        WarnL << "创建套接字失败:" << get_uv_errmsg(true);
        return;
    }
    if (-1 == ioctl(sockfd, SIOCGIFCONF, &ifconf)) {    //获取所有接口信息
        WarnL << "ioctl 失败:" << get_uv_errmsg(true);
        close(sockfd);
        return;
    }
    close(sockfd);
    //接下来一个一个的获取IP地址
    struct ifreq * adapter = (struct ifreq*) buf;
    for (int i = (ifconf.ifc_len / sizeof(struct ifreq)); i > 0; --i,++adapter) {
        if(fun(adapter)){
            break;
        }
    }
}
#endif //!defined(_WIN32) && !defined(__APPLE__)

bool check_ip(string &address,const string &ip){
    if(ip != "127.0.0.1" && ip != "0.0.0.0") {
        /*获取一个有效IP*/
        address = ip;
        uint32_t addressInNetworkOrder = htonl(inet_addr(ip.data()));
        if(/*(addressInNetworkOrder >= 0x0A000000 && addressInNetworkOrder < 0x0E000000) ||*/
           (addressInNetworkOrder >= 0xAC100000 && addressInNetworkOrder < 0xAC200000) ||
           (addressInNetworkOrder >= 0xC0A80000 && addressInNetworkOrder < 0xC0A90000)){
            //A类私有IP地址：
            //10.0.0.0～10.255.255.255
            //B类私有IP地址：
            //172.16.0.0～172.31.255.255
            //C类私有IP地址：
            //192.168.0.0～192.168.255.255
            //如果是私有地址 说明在nat内部

            /* 优先采用局域网地址，该地址很可能是wifi地址
             * 一般来说,无线路由器分配的地址段是BC类私有ip地址
             * 而A类地址多用于蜂窝移动网络
             */
            return true;
        }
    }
    return false;
}

string SockUtil::get_local_ip() {
#if defined(__APPLE__)
    string address = "127.0.0.1";
    for_each_netAdapter_apple([&](struct ifaddrs *adapter){
        string ip = SockUtil::inet_ntoa(((struct sockaddr_in*)adapter->ifa_addr)->sin_addr);
        return check_ip(address,ip);
    });
    return address;
#elif defined(_WIN32)
    string address = "127.0.0.1";
    for_each_netAdapter_win32([&](PIP_ADAPTER_INFO adapter) {
        IP_ADDR_STRING *ipAddr = &(adapter->IpAddressList);
        while (ipAddr) {
            string ip = ipAddr->IpAddress.String;
            if(check_ip(address,ip)){
                return true;
            }
            ipAddr = ipAddr->Next;
        }
        return false;
    });
    return address;
#else
    string address = "127.0.0.1";
    for_each_netAdapter_posix([&](struct ifreq *adapter){
        string ip = SockUtil::inet_ntoa(((struct sockaddr_in*) &(adapter->ifr_addr))->sin_addr);
        return check_ip(address,ip);
    });
    return address;
#endif
}

vector<map<string,string> > SockUtil::getInterfaceList(){
    vector<map<string,string> > ret;
#if defined(__APPLE__)
    for_each_netAdapter_apple([&](struct ifaddrs *adapter){
        map<string,string> obj;
        obj["ip"] = SockUtil::inet_ntoa(((struct sockaddr_in*)adapter->ifa_addr)->sin_addr);
        obj["name"] = adapter->ifa_name;
        ret.emplace_back(std::move(obj));
        return false;
    });
#elif defined(_WIN32)
    for_each_netAdapter_win32([&](PIP_ADAPTER_INFO adapter) {
        IP_ADDR_STRING *ipAddr = &(adapter->IpAddressList);
        while (ipAddr) {
            map<string,string> obj;
            obj["ip"] = ipAddr->IpAddress.String;
            obj["name"] = adapter->AdapterName;
            ret.emplace_back(std::move(obj));
            ipAddr = ipAddr->Next;
        }
        return false;
    });
#else
    for_each_netAdapter_posix([&](struct ifreq *adapter){
        map<string,string> obj;
        obj["ip"] = SockUtil::inet_ntoa(((struct sockaddr_in*) &(adapter->ifr_addr))->sin_addr);
        obj["name"] = adapter->ifr_name;
        ret.emplace_back(std::move(obj));
        return false;
    });
#endif
    return ret;
};

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
            return SockUtil::inet_ntoa(addr_v4->sin_addr);
        }
    }
    return "";
}

int SockUtil::bindSock(int sockFd,const char *ifr_ip,uint16_t port){
    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(ifr_ip);
    bzero(&(servaddr.sin_zero), sizeof servaddr.sin_zero);
    //绑定监听
    if (::bind(sockFd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) {
        WarnL << "绑定套接字失败:" << get_uv_errmsg(true);
        return -1;
    }
    return 0;
}

int SockUtil::bindUdpSock(const uint16_t port, const char* localIp) {
    int sockfd = -1;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        WarnL << "创建套接字失败:" << get_uv_errmsg(true);
        return -1;
    }
    setReuseable(sockfd);
    setNoSigpipe(sockfd);
    setNoBlocked(sockfd);
    setSendBuf(sockfd);
    setRecvBuf(sockfd);
    setCloseWait(sockfd);

    if(bindSock(sockfd,localIp,port) == -1){
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

string SockUtil::get_ifr_ip(const char *ifrName){
#if defined(__APPLE__)
    string ret;
    for_each_netAdapter_apple([&](struct ifaddrs *adapter){
        if(strcmp(adapter->ifa_name,ifrName) == 0) {
            ret = SockUtil::inet_ntoa(((struct sockaddr_in*)adapter->ifa_addr)->sin_addr);
            return true;
        }
        return false;
    });
    return ret;
#elif defined(_WIN32)
    string ret;
    for_each_netAdapter_win32([&](PIP_ADAPTER_INFO adapter) {
        IP_ADDR_STRING *ipAddr = &(adapter->IpAddressList);
        while (ipAddr){
            if (strcmp(ifrName,adapter->AdapterName) == 0){
                //ip匹配到了
                ret.assign(ipAddr->IpAddress.String);
                return true;
            }
            ipAddr = ipAddr->Next;
        }
        return false;
    });
    return ret;
#else
    string ret;
    for_each_netAdapter_posix([&](struct ifreq *adapter){
        if(strcmp(adapter->ifr_name,ifrName) == 0) {
            ret = SockUtil::inet_ntoa(((struct sockaddr_in*) &(adapter->ifr_addr))->sin_addr);
            return true;
        }
        return false;
    });
    return ret;
#endif
}

string SockUtil::get_ifr_name(const char *localIp){
#if defined(__APPLE__)
    string ret = "en0";
    for_each_netAdapter_apple([&](struct ifaddrs *adapter){
        string ip = SockUtil::inet_ntoa(((struct sockaddr_in*)adapter->ifa_addr)->sin_addr);
        if(ip == localIp) {
            ret = adapter->ifa_name;
            return true;
        }
        return false;
    });
    return ret;
#elif defined(_WIN32)
    string ret = "en0";
    for_each_netAdapter_win32([&](PIP_ADAPTER_INFO adapter) {
        IP_ADDR_STRING *ipAddr = &(adapter->IpAddressList);
        while (ipAddr){
            if (strcmp(localIp,ipAddr->IpAddress.String) == 0){
                //ip匹配到了
                ret.assign(adapter->AdapterName);
                return true;
            }
            ipAddr = ipAddr->Next;
        }
        return false;
    });
    return ret;
#else
    string ret = "en0";
    for_each_netAdapter_posix([&](struct ifreq *adapter){
        string ip = SockUtil::inet_ntoa(((struct sockaddr_in*) &(adapter->ifr_addr))->sin_addr);
        if(ip == localIp) {
            ret = adapter->ifr_name;
            return true;
        }
        return false;
    });
    return ret;
#endif
}


string SockUtil::get_ifr_mask(const char* ifrName) {
#if defined(__APPLE__)
    string ret;
    for_each_netAdapter_apple([&](struct ifaddrs *adapter){
        if(strcmp(ifrName,adapter->ifa_name) == 0) {
            ret = SockUtil::inet_ntoa(((struct sockaddr_in *)adapter->ifa_netmask)->sin_addr);
            return true;
        }
        return false;
    });
    return ret;
#elif defined(_WIN32)
    string ret;
    for_each_netAdapter_win32([&](PIP_ADAPTER_INFO adapter) {
        if (strcmp(ifrName,adapter->AdapterName) == 0){
            //找到了该网卡
            IP_ADDR_STRING *ipAddr = &(adapter->IpAddressList);
            //获取第一个ip的子网掩码
            ret.assign(ipAddr->IpMask.String);
            return true;
        }
        return false;
    });
    return ret;
#else
    int sockFd;
    struct ifreq ifr_mask;
    sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFd == -1) {
        WarnL << "创建套接字失败:" << get_uv_errmsg(true);
        return "";
    }
    memset(&ifr_mask, 0, sizeof(ifr_mask));
    strncpy(ifr_mask.ifr_name, ifrName, sizeof(ifr_mask.ifr_name) - 1);
    if ((ioctl(sockFd, SIOCGIFNETMASK, &ifr_mask)) < 0) {
        WarnL << "ioctl 失败:" << ifrName << " " << get_uv_errmsg(true);
        close(sockFd);
        return "";
    }
    close(sockFd);
    return SockUtil::inet_ntoa(((struct sockaddr_in *) &(ifr_mask.ifr_netmask))->sin_addr);
#endif // defined(_WIN32)
}

string SockUtil::get_ifr_brdaddr(const char *ifrName){
#if defined(__APPLE__)
    string ret;
    for_each_netAdapter_apple([&](struct ifaddrs *adapter){
        if(strcmp(ifrName,adapter->ifa_name) == 0){
            ret = SockUtil::inet_ntoa(((struct sockaddr_in*) adapter->ifa_broadaddr)->sin_addr);
            return true;
        }
        return false;
    });
    return ret;
#elif defined(_WIN32)
    string ret;
    for_each_netAdapter_win32([&](PIP_ADAPTER_INFO adapter) {
        if (strcmp(ifrName, adapter->AdapterName) == 0) {
            //找到该网卡
            IP_ADDR_STRING *ipAddr = &(adapter->IpAddressList);
            in_addr broadcast;
            broadcast.S_un.S_addr = (inet_addr(ipAddr->IpAddress.String) & inet_addr(ipAddr->IpMask.String)) | (~inet_addr(ipAddr->IpMask.String));
            ret = SockUtil::inet_ntoa(broadcast);
            return true;
        }
        return false;
    });
    return ret;
#else
    int sockFd;
    struct ifreq ifr_mask;
    sockFd = socket( AF_INET, SOCK_STREAM, 0);
    if (sockFd == -1) {
        WarnL << "创建套接字失败:" << get_uv_errmsg(true);
        return "";
    }
    memset(&ifr_mask, 0, sizeof(ifr_mask));
    strncpy(ifr_mask.ifr_name, ifrName, sizeof(ifr_mask.ifr_name) - 1);
    if ((ioctl(sockFd, SIOCGIFBRDADDR, &ifr_mask)) < 0) {
        WarnL << "ioctl 失败:" << get_uv_errmsg(true);
        close(sockFd);
        return "";
    }
    close(sockFd);
    return SockUtil::inet_ntoa(((struct sockaddr_in *) &(ifr_mask.ifr_broadaddr))->sin_addr);
#endif
}

#define ip_addr_netcmp(addr1, addr2, mask) (((addr1) & (mask)) == ((addr2) & (mask)))
bool SockUtil::in_same_lan(const char *myIp,const char *dstIp){
    string mask = get_ifr_mask(get_ifr_name(myIp).data());
    return ip_addr_netcmp(inet_addr(myIp),inet_addr(dstIp),inet_addr(mask.data()));
}


static void clearMulticastAllSocketOption(int socket) {
#if defined(IP_MULTICAST_ALL)
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
#if defined(IP_MULTICAST_TTL)
    ret= setsockopt(sockFd, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl, sizeof(ttl));
    if (ret == -1) {
        TraceL << "设置 IP_MULTICAST_TTL 失败!";
    }
#endif
    clearMulticastAllSocketOption(sockFd);
    return ret;
}

int SockUtil::setMultiIF(int sockFd, const char* strLocalIp) {
    int ret = -1;
#if defined(IP_MULTICAST_IF)
    struct in_addr addr;
    addr.s_addr = inet_addr(strLocalIp);
    ret = setsockopt(sockFd, IPPROTO_IP, IP_MULTICAST_IF, (char*)&addr, sizeof(addr));
    if (ret == -1) {
        TraceL << "设置 IP_MULTICAST_IF 失败!";
    }
#endif
    clearMulticastAllSocketOption(sockFd);
    return ret;
}

int SockUtil::setMultiLOOP(int sockFd, bool bAccept) {
    int ret = -1;
#if defined(IP_MULTICAST_LOOP)
    uint8_t loop = bAccept;
    ret = setsockopt(sockFd, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&loop, sizeof(loop));
    if (ret == -1) {
        TraceL << "设置 IP_MULTICAST_LOOP 失败!";
    }
#endif
    clearMulticastAllSocketOption(sockFd);
    return ret;
}

int SockUtil::joinMultiAddr(int sockFd, const char* strAddr,const char* strLocalIp) {
    int ret = -1;
#if defined(IP_ADD_MEMBERSHIP)
    struct ip_mreq imr;
    imr.imr_multiaddr.s_addr = inet_addr(strAddr);
    imr.imr_interface.s_addr = inet_addr(strLocalIp);
    ret = setsockopt(sockFd, IPPROTO_IP, IP_ADD_MEMBERSHIP,  (char*)&imr, sizeof (struct ip_mreq));
    if (ret == -1) {
        TraceL << "设置 IP_ADD_MEMBERSHIP 失败:" << get_uv_errmsg(true);
    }
#endif
    clearMulticastAllSocketOption(sockFd);
    return ret;
}

int SockUtil::leaveMultiAddr(int sockFd, const char* strAddr,const char* strLocalIp) {
    int ret = -1;
#if defined(IP_DROP_MEMBERSHIP)
    struct ip_mreq imr;
    imr.imr_multiaddr.s_addr = inet_addr(strAddr);
    imr.imr_interface.s_addr = inet_addr(strLocalIp);
    ret = setsockopt(sockFd, IPPROTO_IP, IP_DROP_MEMBERSHIP,  (char*)&imr, sizeof (struct ip_mreq));
    if (ret == -1) {
        TraceL << "设置 IP_DROP_MEMBERSHIP 失败:" << get_uv_errmsg(true);
    }
#endif
    clearMulticastAllSocketOption(sockFd);
    return ret;
}

template <typename A,typename B>
static inline void write4Byte(A &&a,B &&b){
    memcpy(&a,&b, sizeof(a));
}

int SockUtil::joinMultiAddrFilter(int sockFd, const char* strAddr,
        const char* strSrcIp, const char* strLocalIp) {
    int ret = -1;
#if defined(IP_ADD_SOURCE_MEMBERSHIP)
    struct ip_mreq_source imr;

    write4Byte(imr.imr_multiaddr,inet_addr(strAddr));
    write4Byte(imr.imr_sourceaddr,inet_addr(strSrcIp));
    write4Byte(imr.imr_interface,inet_addr(strLocalIp));

    ret = setsockopt(sockFd, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP,
            (char*) &imr, sizeof(struct ip_mreq_source));
    if (ret == -1) {
        TraceL << "设置 IP_ADD_SOURCE_MEMBERSHIP 失败:" << get_uv_errmsg(true);
    }
#endif
    clearMulticastAllSocketOption(sockFd);
    return ret;
}

int SockUtil::leaveMultiAddrFilter(int sockFd, const char* strAddr,
        const char* strSrcIp, const char* strLocalIp) {
    int ret = -1;
#if defined(IP_DROP_SOURCE_MEMBERSHIP)
    struct ip_mreq_source imr;

    write4Byte(imr.imr_multiaddr,inet_addr(strAddr));
    write4Byte(imr.imr_sourceaddr,inet_addr(strSrcIp));
    write4Byte(imr.imr_interface,inet_addr(strLocalIp));

    ret = setsockopt(sockFd, IPPROTO_IP, IP_DROP_SOURCE_MEMBERSHIP,
            (char*) &imr, sizeof(struct ip_mreq_source));
    if (ret == -1) {
        TraceL << "设置 IP_DROP_SOURCE_MEMBERSHIP 失败:" << get_uv_errmsg(true);
    }
#endif
    clearMulticastAllSocketOption(sockFd);
    return ret;
}



}  // namespace toolkit


