/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */
#include <fcntl.h>
#include <assert.h>
#include <cstdio>
#include <cstring>
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
#if (_WIN32_WINNT < _WIN32_WINNT_VISTA)
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size) {
    struct sockaddr_storage ss;
    unsigned long s = size;

    ZeroMemory(&ss, sizeof(ss));
    ss.ss_family = af;

    switch (af) {
    case AF_INET:
        ((struct sockaddr_in *)&ss)->sin_addr = *(struct in_addr *)src;
        break;
    case AF_INET6:
        ((struct sockaddr_in6 *)&ss)->sin6_addr = *(struct in6_addr *)src;
        break;
    default:
        return NULL;
    }
    /* cannot direclty use &size because of strict aliasing rules */
    return (WSAAddressToString((struct sockaddr *)&ss, sizeof(ss), NULL, dst, &s) == 0) ? dst : NULL;
}
int inet_pton(int af, const char *src, void *dst) {
    struct sockaddr_storage ss;
    int size = sizeof(ss);
    char src_copy[INET6_ADDRSTRLEN + 1];

    ZeroMemory(&ss, sizeof(ss));
    /* stupid non-const API */
    strncpy(src_copy, src, INET6_ADDRSTRLEN + 1);
    src_copy[INET6_ADDRSTRLEN] = 0;

    if (WSAStringToAddress(src_copy, af, NULL, (struct sockaddr *)&ss, &size) == 0) {
        switch (af) {
        case AF_INET:
            *(struct in_addr *)dst = ((struct sockaddr_in *)&ss)->sin_addr;
            return 1;
        case AF_INET6:
            *(struct in6_addr *)dst = ((struct sockaddr_in6 *)&ss)->sin6_addr;
            return 1;
        }
    }
    return 0;
}
#endif
#endif // defined(_WIN32)

static inline string my_inet_ntop(int af, const void *addr) {
    string ret;
    ret.resize(128);
    if (!inet_ntop(af, const_cast<void*>(addr), (char *) ret.data(), ret.size())) {
        ret.clear();
    } else {
        ret.resize(strlen(ret.data()));
    }
    return ret;
}

static inline bool support_ipv6_l() {
    auto fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (fd == -1) {
        return false;
    }
    close(fd);
    return true;
}

static inline bool support_ipv6() {
    static auto flag = support_ipv6_l();
    return flag;
}

string SockUtil::inet_ntoa(const struct in_addr &addr) {
    return my_inet_ntop(AF_INET, &addr);
}

std::string SockUtil::inet_ntoa(const struct in6_addr &addr) {
    return my_inet_ntop(AF_INET6, &addr);
}

std::string SockUtil::inet_ntoa(const struct sockaddr *addr) {
    switch (addr->sa_family) {
        case AF_INET: return SockUtil::inet_ntoa(((struct sockaddr_in *)addr)->sin_addr);
        case AF_INET6: {
            if (IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6 *)addr)->sin6_addr)) {
                struct in_addr addr4;
                memcpy(&addr4, 12 + (char *)&(((struct sockaddr_in6 *)addr)->sin6_addr), 4);
                return SockUtil::inet_ntoa(addr4);
            }
            return SockUtil::inet_ntoa(((struct sockaddr_in6 *)addr)->sin6_addr);
        }
        default: assert(false); return "";
    }
}

uint16_t SockUtil::inet_port(const struct sockaddr *addr) {
    switch (addr->sa_family) {
        case AF_INET: return ntohs(((struct sockaddr_in *)addr)->sin_port);
        case AF_INET6: return ntohs(((struct sockaddr_in6 *)addr)->sin6_port);
        default: assert(false); return 0;
    }
}

int SockUtil::setCloseWait(int fd, int second) {
    linger m_sLinger;
    //在调用closesocket()时还有数据未发送完，允许等待
    // 若m_sLinger.l_onoff=0;则调用closesocket()后强制关闭
    m_sLinger.l_onoff = (second > 0);
    m_sLinger.l_linger = second; //设置等待时间为x秒
    int ret = setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &m_sLinger, sizeof(linger));
    if (ret == -1) {
#ifndef _WIN32
        TraceL << "设置 SO_LINGER 失败!";
#endif
    }
    return ret;
}

int SockUtil::setNoDelay(int fd, bool on) {
    int opt = on ? 1 : 0;
    int ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &opt, static_cast<socklen_t>(sizeof(opt)));
    if (ret == -1) {
        TraceL << "设置 NoDelay 失败!";
    }
    return ret;
}

int SockUtil::setReuseable(int fd, bool on, bool reuse_port) {
    int opt = on ? 1 : 0;
    int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, static_cast<socklen_t>(sizeof(opt)));
    if (ret == -1) {
        TraceL << "设置 SO_REUSEADDR 失败!";
        return ret;
    }
#if defined(SO_REUSEPORT)
    if (reuse_port) {
        ret = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (char *) &opt, static_cast<socklen_t>(sizeof(opt)));
        if (ret == -1) {
            TraceL << "设置 SO_REUSEPORT 失败!";
        }
    }
#endif
    return ret;
}

int SockUtil::setBroadcast(int fd, bool on) {
    int opt = on ? 1 : 0;
    int ret = setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (char *) &opt, static_cast<socklen_t>(sizeof(opt)));
    if (ret == -1) {
        TraceL << "设置 SO_BROADCAST 失败!";
    }
    return ret;
}

int SockUtil::setKeepAlive(int fd, bool on) {
    int opt = on ? 1 : 0;
    int ret = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *) &opt, static_cast<socklen_t>(sizeof(opt)));
    if (ret == -1) {
        TraceL << "设置 SO_KEEPALIVE 失败!";
    }
    return ret;
}

int SockUtil::setCloExec(int fd, bool on) {
#if !defined(_WIN32)
    int flags = fcntl(fd, F_GETFD);
    if (flags == -1) {
        TraceL << "设置 FD_CLOEXEC 失败!";
        return -1;
    }
    if (on) {
        flags |= FD_CLOEXEC;
    } else {
        int cloexec = FD_CLOEXEC;
        flags &= ~cloexec;
    }
    int ret = fcntl(fd, F_SETFD, flags);
    if (ret == -1) {
        TraceL << "设置 FD_CLOEXEC 失败!";
        return -1;
    }
    return ret;
#else
    return -1;
#endif
}

int SockUtil::setNoSigpipe(int fd) {
#if defined(SO_NOSIGPIPE)
    int set = 1;
    auto ret = setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (char *) &set, sizeof(int));
    if (ret == -1) {
        TraceL << "设置 SO_NOSIGPIPE 失败!";
    }
    return ret;
#else
    return -1;
#endif
}

int SockUtil::setNoBlocked(int fd, bool noblock) {
#if defined(_WIN32)
    unsigned long ul = noblock;
#else
    int ul = noblock;
#endif //defined(_WIN32)
    int ret = ioctl(fd, FIONBIO, &ul); //设置为非阻塞模式
    if (ret == -1) {
        TraceL << "设置非阻塞失败!";
    }

    return ret;
}

int SockUtil::setRecvBuf(int fd, int size) {
    int ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *) &size, sizeof(size));
    if (ret == -1) {
        TraceL << "设置接收缓冲区失败!";
    }
    return ret;
}

int SockUtil::setSendBuf(int fd, int size) {
    int ret = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *) &size, sizeof(size));
    if (ret == -1) {
        TraceL << "设置发送缓冲区失败!";
    }
    return ret;
}

class DnsCache {
public:
    static DnsCache &Instance() {
        static DnsCache instance;
        return instance;
    }

    bool getDomainIP(const char *host, sockaddr_storage &storage, int ai_family = AF_INET,
                     int ai_socktype = SOCK_STREAM, int ai_protocol = IPPROTO_TCP, int expire_sec = 60) {
        try {
            storage = SockUtil::make_sockaddr(host, 0);
            return true;
        } catch (...) {
            auto item = getCacheDomainIP(host, expire_sec);
            if (!item) {
                item = getSystemDomainIP(host);
                if (item) {
                    setCacheDomainIP(host, item);
                }
            }
            if (item) {
                auto addr = getPerferredAddress(item.get(), ai_family, ai_socktype, ai_protocol);
                memcpy(&storage, addr->ai_addr, addr->ai_addrlen);
            }
            return (bool)item;
        }
    }

private:
    class DnsItem {
    public:
        std::shared_ptr<struct addrinfo> addr_info;
        time_t create_time;
    };

    std::shared_ptr<struct addrinfo> getCacheDomainIP(const char *host, int expireSec) {
        lock_guard<mutex> lck(_mtx);
        auto it = _dns_cache.find(host);
        if (it == _dns_cache.end()) {
            //没有记录
            return nullptr;
        }
        if (it->second.create_time + expireSec < time(nullptr)) {
            //已过期
            _dns_cache.erase(it);
            return nullptr;
        }
        return it->second.addr_info;
    }

    void setCacheDomainIP(const char *host, std::shared_ptr<struct addrinfo> addr) {
        lock_guard<mutex> lck(_mtx);
        DnsItem item;
        item.addr_info = std::move(addr);
        item.create_time = time(nullptr);
        _dns_cache[host] = std::move(item);
    }

    std::shared_ptr<struct addrinfo> getSystemDomainIP(const char *host) {
        struct addrinfo *answer = nullptr;
        //阻塞式dns解析，可能被打断
        int ret = -1;
        do {
            ret = getaddrinfo(host, nullptr, nullptr, &answer);
        } while (ret == -1 && get_uv_error(true) == UV_EINTR);

        if (!answer) {
            WarnL << "域名解析失败:" << host;
            return nullptr;
        }
        return std::shared_ptr<struct addrinfo>(answer, freeaddrinfo);
    }

    struct addrinfo *getPerferredAddress(struct addrinfo *answer, int ai_family, int ai_socktype, int ai_protocol) {
        auto ptr = answer;
        while (ptr) {
            if (ptr->ai_family == ai_family && ptr->ai_socktype == ai_socktype && ptr->ai_protocol == ai_protocol) {
                return ptr;
            }
            ptr = ptr->ai_next;
        }
        return answer;
    }

private:
    mutex _mtx;
    unordered_map<string, DnsItem> _dns_cache;
};

bool SockUtil::getDomainIP(const char *host, uint16_t port, struct sockaddr_storage &addr,
                           int ai_family, int ai_socktype, int ai_protocol, int expire_sec) {
    bool flag = DnsCache::Instance().getDomainIP(host, addr, ai_family, ai_socktype, ai_protocol, expire_sec);
    if (flag) {
        switch (addr.ss_family ) {
            case AF_INET : ((sockaddr_in *) &addr)->sin_port = htons(port); break;
            case AF_INET6 : ((sockaddr_in6 *) &addr)->sin6_port = htons(port); break;
            default: assert(0); break;
        }
    }
    return flag;
}

static int set_ipv6_only(int fd, bool flag) {
    int opt = flag;
    int ret = setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&opt, sizeof opt);
    if (ret == -1) {
        TraceL << "设置 IPV6_V6ONLY 失败!";
    }
    return ret;
}

static int bind_sock6(int fd, const char *ifr_ip, uint16_t port) {
    set_ipv6_only(fd, false);
    struct sockaddr_in6 addr;
    bzero(&addr, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
    if (1 != inet_pton(AF_INET6, ifr_ip, &(addr.sin6_addr))) {
        if (strcmp(ifr_ip, "0.0.0.0")) {
            WarnL << "inet_pton to ipv6 address failed:" << ifr_ip;
        }
        addr.sin6_addr = IN6ADDR_ANY_INIT;
    }
    if (::bind(fd, (struct sockaddr *) &addr, SockUtil::get_sock_len((struct sockaddr *) &addr)) == -1) {
        WarnL << "绑定套接字失败:" << get_uv_errmsg(true);
        return -1;
    }
    return 0;
}

static int bind_sock4(int fd, const char *ifr_ip, uint16_t port) {
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (1 != inet_pton(AF_INET, ifr_ip, &(addr.sin_addr))) {
        if (strcmp(ifr_ip, "::")) {
            WarnL << "inet_pton to ipv4 address failed:" << ifr_ip;
        }
        addr.sin_addr.s_addr = INADDR_ANY;
    }
    if (::bind(fd, (struct sockaddr *) &addr, SockUtil::get_sock_len((struct sockaddr *) &addr)) == -1) {
        WarnL << "绑定套接字失败:" << get_uv_errmsg(true);
        return -1;
    }
    return 0;
}

static int bind_sock(int fd, const char *ifr_ip, uint16_t port, int family) {
    switch (family) {
        case AF_INET: return bind_sock4(fd, ifr_ip, port);
        case AF_INET6: return bind_sock6(fd, ifr_ip, port);
        default: assert(0); return -1;
    }
}

int SockUtil::connect(const char *host, uint16_t port, bool async, const char *local_ip, uint16_t local_port) {
    sockaddr_storage addr;
    //优先使用ipv4地址
    if (!getDomainIP(host, port, addr, AF_INET, SOCK_STREAM, IPPROTO_TCP)) {
        //dns解析失败
        return -1;
    }

    int sockfd = (int) socket(addr.ss_family, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        WarnL << "创建套接字失败:" << host;
        return -1;
    }

    setReuseable(sockfd);
    setNoSigpipe(sockfd);
    setNoBlocked(sockfd, async);
    setNoDelay(sockfd);
    setSendBuf(sockfd);
    setRecvBuf(sockfd);
    setCloseWait(sockfd);
    setCloExec(sockfd);

    if (bind_sock(sockfd, local_ip, local_port, addr.ss_family) == -1) {
        close(sockfd);
        return -1;
    }

    if (::connect(sockfd, (sockaddr *) &addr, get_sock_len((sockaddr *)&addr)) == 0) {
        //同步连接成功
        return sockfd;
    }
    if (async && get_uv_error(true) == UV_EAGAIN) {
        //异步连接成功
        return sockfd;
    }
    WarnL << "连接主机失败:" << host << " " << port << " " << get_uv_errmsg(true);
    close(sockfd);
    return -1;
}

int SockUtil::listen(const uint16_t port, const char *local_ip, int back_log) {
    int fd = -1;
    int family = support_ipv6() ? (is_ipv4(local_ip) ? AF_INET : AF_INET6) : AF_INET;
    if ((fd = (int)socket(family, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        WarnL << "创建套接字失败:" << get_uv_errmsg(true);
        return -1;
    }

    setReuseable(fd, true, false);
    setNoBlocked(fd);
    setCloExec(fd);

    if (bind_sock(fd, local_ip, port, family) == -1) {
        close(fd);
        return -1;
    }

    //开始监听
    if (::listen(fd, back_log) == -1) {
        WarnL << "开始监听失败:" << get_uv_errmsg(true);
        close(fd);
        return -1;
    }

    return fd;
}

int SockUtil::getSockError(int fd) {
    int opt;
    socklen_t optLen = static_cast<socklen_t>(sizeof(opt));

    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *) &opt, &optLen) < 0) {
        return get_uv_error(true);
    } else {
        return uv_translate_posix_error(opt);
    }
}

using getsockname_type = decltype(getsockname);

static string get_socket_ip(int fd, getsockname_type func) {
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);
    if (-1 == func(fd, (struct sockaddr *)&addr, &addr_len)) {
        return "";
    }
    return SockUtil::inet_ntoa((struct sockaddr *)&addr);
}

static uint16_t get_socket_port(int fd, getsockname_type func) {
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);
    if (-1 == func(fd, (struct sockaddr *)&addr, &addr_len)) {
        return 0;
    }
    return SockUtil::inet_port((struct sockaddr *)&addr);
}

string SockUtil::get_local_ip(int fd) {
    return get_socket_ip(fd, getsockname);
}

string SockUtil::get_peer_ip(int fd) {
    return get_socket_ip(fd, getpeername);
}

uint16_t SockUtil::get_local_port(int fd) {
    return get_socket_port(fd, getsockname);
}

uint16_t SockUtil::get_peer_port(int fd) {
    return get_socket_port(fd, getpeername);
}

#if defined(__APPLE__)
template<typename FUN>
void for_each_netAdapter_apple(FUN &&fun) { //type: struct ifaddrs *
    struct ifaddrs *interfaces = nullptr;
    struct ifaddrs *adapter = nullptr;
    if (getifaddrs(&interfaces) == 0) {
        adapter = interfaces;
        while (adapter) {
            if (adapter->ifa_addr->sa_family == AF_INET) {
                if (fun(adapter)) {
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

bool check_ip(string &address, const string &ip) {
    if (ip != "127.0.0.1" && ip != "0.0.0.0") {
        /*获取一个有效IP*/
        address = ip;
        uint32_t addressInNetworkOrder = htonl(inet_addr(ip.data()));
        if (/*(addressInNetworkOrder >= 0x0A000000 && addressInNetworkOrder < 0x0E000000) ||*/
            (addressInNetworkOrder >= 0xAC100000 && addressInNetworkOrder < 0xAC200000) ||
            (addressInNetworkOrder >= 0xC0A80000 && addressInNetworkOrder < 0xC0A90000)) {
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
    for_each_netAdapter_apple([&](struct ifaddrs *adapter) {
        string ip = SockUtil::inet_ntoa(adapter->ifa_addr);
        if (strstr(adapter->ifa_name, "docker")) {
            return false;
        }
        return check_ip(address, ip);
    });
    return address;
#elif defined(_WIN32)
    string address = "127.0.0.1";
    for_each_netAdapter_win32([&](PIP_ADAPTER_INFO adapter) {
        IP_ADDR_STRING *ipAddr = &(adapter->IpAddressList);
        while (ipAddr) {
            string ip = ipAddr->IpAddress.String;
            if (strstr(adapter->AdapterName, "docker")) {
                return false;
            }
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
        string ip = SockUtil::inet_ntoa(&(adapter->ifr_addr));
        if (strstr(adapter->ifr_name, "docker")) {
            return false;
        }
        return check_ip(address,ip);
    });
    return address;
#endif
}

vector<map<string, string> > SockUtil::getInterfaceList() {
    vector<map<string, string> > ret;
#if defined(__APPLE__)
    for_each_netAdapter_apple([&](struct ifaddrs *adapter) {
        map<string, string> obj;
        obj["ip"] = SockUtil::inet_ntoa(adapter->ifa_addr);
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
        obj["ip"] = SockUtil::inet_ntoa(&(adapter->ifr_addr));
        obj["name"] = adapter->ifr_name;
        ret.emplace_back(std::move(obj));
        return false;
    });
#endif
    return ret;
}

int SockUtil::bindUdpSock(const uint16_t port, const char *local_ip, bool enable_reuse) {
    int fd = -1;
    int family = support_ipv6() ? (is_ipv4(local_ip) ? AF_INET : AF_INET6) : AF_INET;
    if ((fd = (int)socket(family, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        WarnL << "创建套接字失败:" << get_uv_errmsg(true);
        return -1;
    }
    if (enable_reuse) {
        setReuseable(fd);
    }
    setNoSigpipe(fd);
    setNoBlocked(fd);
    setSendBuf(fd);
    setRecvBuf(fd);
    setCloseWait(fd);
    setCloExec(fd);

    if (bind_sock(fd, local_ip, port, family) == -1) {
        close(fd);
        return -1;
    }
    return fd;
}

int SockUtil::dissolveUdpSock(int fd) {
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);
    if (-1 == getsockname(fd, (struct sockaddr *)&addr, &addr_len)) {
        return -1;
    }
    addr.ss_family = AF_UNSPEC;
    if (-1 == ::connect(fd, (struct sockaddr *)&addr, addr_len) && get_uv_error() != UV_EAFNOSUPPORT) {
        // mac/ios时返回EAFNOSUPPORT错误
        WarnL << "解除 UDP 套接字连接关系失败: " << get_uv_errmsg(true);
        return -1;
    }
   return 0;
}

string SockUtil::get_ifr_ip(const char *if_name) {
#if defined(__APPLE__)
    string ret;
    for_each_netAdapter_apple([&](struct ifaddrs *adapter) {
        if (strcmp(adapter->ifa_name, if_name) == 0) {
            ret = SockUtil::inet_ntoa(adapter->ifa_addr);
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
            if (strcmp(if_name,adapter->AdapterName) == 0){
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
        if(strcmp(adapter->ifr_name,if_name) == 0) {
            ret = SockUtil::inet_ntoa(&(adapter->ifr_addr));
            return true;
        }
        return false;
    });
    return ret;
#endif
}

string SockUtil::get_ifr_name(const char *local_ip) {
#if defined(__APPLE__)
    string ret = "en0";
    for_each_netAdapter_apple([&](struct ifaddrs *adapter) {
        string ip = SockUtil::inet_ntoa(adapter->ifa_addr);
        if (ip == local_ip) {
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
            if (strcmp(local_ip,ipAddr->IpAddress.String) == 0){
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
        string ip = SockUtil::inet_ntoa(&(adapter->ifr_addr));
        if(ip == local_ip) {
            ret = adapter->ifr_name;
            return true;
        }
        return false;
    });
    return ret;
#endif
}

string SockUtil::get_ifr_mask(const char *if_name) {
#if defined(__APPLE__)
    string ret;
    for_each_netAdapter_apple([&](struct ifaddrs *adapter) {
        if (strcmp(if_name, adapter->ifa_name) == 0) {
            ret = SockUtil::inet_ntoa(adapter->ifa_netmask);
            return true;
        }
        return false;
    });
    return ret;
#elif defined(_WIN32)
    string ret;
    for_each_netAdapter_win32([&](PIP_ADAPTER_INFO adapter) {
        if (strcmp(if_name,adapter->AdapterName) == 0){
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
    int fd;
    struct ifreq ifr_mask;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        WarnL << "创建套接字失败:" << get_uv_errmsg(true);
        return "";
    }
    memset(&ifr_mask, 0, sizeof(ifr_mask));
    strncpy(ifr_mask.ifr_name, if_name, sizeof(ifr_mask.ifr_name) - 1);
    if ((ioctl(fd, SIOCGIFNETMASK, &ifr_mask)) < 0) {
        WarnL << "ioctl 失败:" << if_name << " " << get_uv_errmsg(true);
        close(fd);
        return "";
    }
    close(fd);
    return SockUtil::inet_ntoa(&(ifr_mask.ifr_netmask));
#endif // defined(_WIN32)
}

string SockUtil::get_ifr_brdaddr(const char *if_name) {
#if defined(__APPLE__)
    string ret;
    for_each_netAdapter_apple([&](struct ifaddrs *adapter) {
        if (strcmp(if_name, adapter->ifa_name) == 0) {
            ret = SockUtil::inet_ntoa(adapter->ifa_broadaddr);
            return true;
        }
        return false;
    });
    return ret;
#elif defined(_WIN32)
    string ret;
    for_each_netAdapter_win32([&](PIP_ADAPTER_INFO adapter) {
        if (strcmp(if_name, adapter->AdapterName) == 0) {
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
    int fd;
    struct ifreq ifr_mask;
    fd = socket( AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        WarnL << "创建套接字失败:" << get_uv_errmsg(true);
        return "";
    }
    memset(&ifr_mask, 0, sizeof(ifr_mask));
    strncpy(ifr_mask.ifr_name, if_name, sizeof(ifr_mask.ifr_name) - 1);
    if ((ioctl(fd, SIOCGIFBRDADDR, &ifr_mask)) < 0) {
        WarnL << "ioctl 失败:" << get_uv_errmsg(true);
        close(fd);
        return "";
    }
    close(fd);
    return SockUtil::inet_ntoa(&(ifr_mask.ifr_broadaddr));
#endif
}

#define ip_addr_netcmp(addr1, addr2, mask) (((addr1) & (mask)) == ((addr2) & (mask)))

bool SockUtil::in_same_lan(const char *myIp, const char *dstIp) {
    string mask = get_ifr_mask(get_ifr_name(myIp).data());
    return ip_addr_netcmp(inet_addr(myIp), inet_addr(dstIp), inet_addr(mask.data()));
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

int SockUtil::setMultiTTL(int fd, uint8_t ttl) {
    int ret = -1;
#if defined(IP_MULTICAST_TTL)
    ret = setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, (char *) &ttl, sizeof(ttl));
    if (ret == -1) {
        TraceL << "设置 IP_MULTICAST_TTL 失败!";
    }
#endif
    clearMulticastAllSocketOption(fd);
    return ret;
}

int SockUtil::setMultiIF(int fd, const char *local_ip) {
    int ret = -1;
#if defined(IP_MULTICAST_IF)
    struct in_addr addr;
    addr.s_addr = inet_addr(local_ip);
    ret = setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, (char *) &addr, sizeof(addr));
    if (ret == -1) {
        TraceL << "设置 IP_MULTICAST_IF 失败!";
    }
#endif
    clearMulticastAllSocketOption(fd);
    return ret;
}

int SockUtil::setMultiLOOP(int fd, bool accept) {
    int ret = -1;
#if defined(IP_MULTICAST_LOOP)
    uint8_t loop = accept;
    ret = setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, (char *) &loop, sizeof(loop));
    if (ret == -1) {
        TraceL << "设置 IP_MULTICAST_LOOP 失败!";
    }
#endif
    clearMulticastAllSocketOption(fd);
    return ret;
}

int SockUtil::joinMultiAddr(int fd, const char *addr, const char *local_ip) {
    int ret = -1;
#if defined(IP_ADD_MEMBERSHIP)
    struct ip_mreq imr;
    imr.imr_multiaddr.s_addr = inet_addr(addr);
    imr.imr_interface.s_addr = inet_addr(local_ip);
    ret = setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &imr, sizeof(struct ip_mreq));
    if (ret == -1) {
        TraceL << "设置 IP_ADD_MEMBERSHIP 失败:" << get_uv_errmsg(true);
    }
#endif
    clearMulticastAllSocketOption(fd);
    return ret;
}

int SockUtil::leaveMultiAddr(int fd, const char *addr, const char *local_ip) {
    int ret = -1;
#if defined(IP_DROP_MEMBERSHIP)
    struct ip_mreq imr;
    imr.imr_multiaddr.s_addr = inet_addr(addr);
    imr.imr_interface.s_addr = inet_addr(local_ip);
    ret = setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *) &imr, sizeof(struct ip_mreq));
    if (ret == -1) {
        TraceL << "设置 IP_DROP_MEMBERSHIP 失败:" << get_uv_errmsg(true);
    }
#endif
    clearMulticastAllSocketOption(fd);
    return ret;
}

template<typename A, typename B>
static inline void write4Byte(A &&a, B &&b) {
    memcpy(&a, &b, sizeof(a));
}

int SockUtil::joinMultiAddrFilter(int fd, const char *addr, const char *src_ip, const char *local_ip) {
    int ret = -1;
#if defined(IP_ADD_SOURCE_MEMBERSHIP)
    struct ip_mreq_source imr;

    write4Byte(imr.imr_multiaddr, inet_addr(addr));
    write4Byte(imr.imr_sourceaddr, inet_addr(src_ip));
    write4Byte(imr.imr_interface, inet_addr(local_ip));

    ret = setsockopt(fd, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP, (char *) &imr, sizeof(struct ip_mreq_source));
    if (ret == -1) {
        TraceL << "设置 IP_ADD_SOURCE_MEMBERSHIP 失败:" << get_uv_errmsg(true);
    }
#endif
    clearMulticastAllSocketOption(fd);
    return ret;
}

int SockUtil::leaveMultiAddrFilter(int fd, const char *addr, const char *src_ip, const char *local_ip) {
    int ret = -1;
#if defined(IP_DROP_SOURCE_MEMBERSHIP)
    struct ip_mreq_source imr;

    write4Byte(imr.imr_multiaddr, inet_addr(addr));
    write4Byte(imr.imr_sourceaddr, inet_addr(src_ip));
    write4Byte(imr.imr_interface, inet_addr(local_ip));

    ret = setsockopt(fd, IPPROTO_IP, IP_DROP_SOURCE_MEMBERSHIP, (char *) &imr, sizeof(struct ip_mreq_source));
    if (ret == -1) {
        TraceL << "设置 IP_DROP_SOURCE_MEMBERSHIP 失败:" << get_uv_errmsg(true);
    }
#endif
    clearMulticastAllSocketOption(fd);
    return ret;
}

bool SockUtil::is_ipv4(const char *host) {
    struct in_addr addr;
    return 1 == inet_pton(AF_INET, host, &addr);
}

bool SockUtil::is_ipv6(const char *host) {
    struct in6_addr addr;
    return 1 == inet_pton(AF_INET6, host, &addr);
}

socklen_t SockUtil::get_sock_len(const struct sockaddr *addr) {
    switch (addr->sa_family) {
        case AF_INET : return sizeof(sockaddr_in);
        case AF_INET6 : return sizeof(sockaddr_in6);
        default: assert(0); return 0;
    }
}

struct sockaddr_storage SockUtil::make_sockaddr(const char *host, uint16_t port) {
    struct sockaddr_storage storage;
    bzero(&storage, sizeof(storage));

    struct in_addr addr;
    struct in6_addr addr6;
    if (1 == inet_pton(AF_INET, host, &addr)) {
        // host是ipv4
        reinterpret_cast<struct sockaddr_in &>(storage).sin_addr = addr;
        reinterpret_cast<struct sockaddr_in &>(storage).sin_family = AF_INET;
        reinterpret_cast<struct sockaddr_in &>(storage).sin_port = htons(port);
        return storage;
    }
    if (1 == inet_pton(AF_INET6, host, &addr6)) {
        // host是ipv6
        reinterpret_cast<struct sockaddr_in6 &>(storage).sin6_addr = addr6;
        reinterpret_cast<struct sockaddr_in6 &>(storage).sin6_family = AF_INET6;
        reinterpret_cast<struct sockaddr_in6 &>(storage).sin6_port = htons(port);
        return storage;
    }
    throw std::invalid_argument(string("not ip address:") + host);
}

}  // namespace toolkit