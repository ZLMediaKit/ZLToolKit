/*
 * util.h
 *
 *  Created on: 2016年8月4日
 *      Author: xzl
 */

#ifndef UTIL_UTIL_H_
#define UTIL_UTIL_H_

#if defined(WIN32)
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4819)
#include <WinSock2.h>
#include <Iphlpapi.h>
#pragma comment(lib,"Iphlpapi.lib") //需要添加Iphlpapi.lib库
#pragma  comment (lib,"WS2_32") 
#else
#include <dirent.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif // defined(WIN32)

#include <stdio.h>
#include <string.h>
#include <string>
#include <sstream>

using namespace std;

namespace ZL {
namespace Util {

class _StrPrinter {
public:
	_StrPrinter() {
	}
	template<typename T>
	_StrPrinter& operator <<(const T& data) {
		ss << data;
		return *this;
	}
	string operator <<(std::ostream&(*f)(std::ostream&)) const {
		return ss.str();
	}
private:
	stringstream ss;
};

#define StrPrinter _StrPrinter()

string makeRandStr(int sz, bool printable = true);
string hexdump(const void *buf, size_t len);
string exePath();
string exeDir();
string exeName();
void setExePath(const string &path);

#ifndef bzero
#define bzero(ptr,size)  memset((ptr),0,(size));
#endif //bzero

#if defined(ANDROID)
template <typename T>
std::string to_string(T value)
{
    std::ostringstream os ;
    os <<  std::forward<T>(value);
    return os.str() ;
}
#endif//ANDROID

#if defined(WIN32)
int strcasecmp(const char *strA,const char *strB);
void sleep(int second);
void usleep(int micro_seconds);

#ifndef socklen_t
#define socklen_t int
#endif //!socklen_t

#ifndef SHUT_RDWR
#define SHUT_RDWR 2
#endif //!SHUT_RDWR

#ifndef PATH_MAX
#define PATH_MAX 256
#endif // !PATH_MAX

struct dirent
{
	long d_ino;              /* inode number*/
	off_t d_off;             /* offset to this dirent*/
	unsigned short d_reclen; /* length of this d_name*/
	unsigned char d_type;    /* the type of d_name*/
	char d_name[1];          /* file name (null-terminated)*/
};

typedef struct _dirdesc {
	int     dd_fd;      /** file descriptor associated with directory */
	long    dd_loc;     /** offset in current buffer */
	long    dd_size;    /** amount of data returned by getdirentries */
	char    *dd_buf;    /** data buffer */
	int     dd_len;     /** size of data buffer */
	long    dd_seek;    /** magic cookie returned by getdirentries */
	struct dirent *index;
} DIR;

# define __dirfd(dp)    ((dp)->dd_fd)

DIR *opendir(const char *);
struct dirent *readdir(DIR *);
void rewinddir(DIR *);
int closedir(DIR *);

int gettimeofday(struct timeval *tp, void *tzp);
int mkdir(const char *path, int mode);

int ioctl(int fd, long cmd , u_long *ptr);
int close(int fd);
#endif //WIN32

}  // namespace Util
}  // namespace ZL

#endif /* UTIL_UTIL_H_ */
