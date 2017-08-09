/*
 * util.h
 *
 *  Created on: 2016年8月4日
 *      Author: xzl
 */


#include <stdlib.h>
#include <mutex>
#include <string>
#include <algorithm>
#include <unordered_map>

#include "util.h"
#include "Util/logger.h"
#include "Util/onceToken.h"

#if defined(_WIN32)
#include <io.h>   
#include <direct.h>  
#include <sys/stat.h>  
#include <sys/types.h>
#include <shlwapi.h>  
#pragma comment(lib, "shlwapi.lib")
#define DIR_SUFFIX '\\'
#else
#define DIR_SUFFIX '/'
#endif // defined(_WIN32)

using namespace std;

namespace ZL {
namespace Util {
string makeRandStr(int sz, bool printable) {
	char *tmp =  new char[sz+1];
	static const char CCH[] =
			"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int i;
	for (i = 0; i < sz; i++) {
		srand((unsigned) time(NULL) + i);
		if (printable) {
			int x = rand() % (sizeof(CCH) - 1);
			tmp[i] = CCH[x];
		} else {
			tmp[i] = rand() % 0xFF;
		}
	}
	tmp[i] = 0;
	string ret = tmp;
	delete [] tmp;
	return ret;
}

bool is_safe(uint8_t b) {
	return b >= ' ' && b < 128;
}
string hexdump(const void *buf, size_t len) {
	string ret("\r\n");
	char tmp[8];
	const uint8_t *data = (const uint8_t *) buf;
	for (size_t i = 0; i < len; i += 16) {
		for (int j = 0; j < 16; ++j) {
			if (i + j < len) {
				int sz = sprintf(tmp, "%.2x ", data[i + j]);
				ret.append(tmp, sz);
			} else {
				int sz = sprintf(tmp, "   ");
				ret.append(tmp, sz);
			}
		}
		for (int j = 0; j < 16; ++j) {
			if (i + j < len) {
				ret += (is_safe(data[i + j]) ? data[i + j] : '.');
			} else {
				ret += (' ');
			}
		}
		ret += ('\n');
	}
	return ret;
}
static string _exePath("./");
string exePath() {
	string filePath;
	char buffer[1024] = {0};
#if defined(_WIN32)
	wchar_t szExePath[MAX_PATH] = { 0 };
	int wcSize = GetModuleFileNameW(NULL, szExePath, sizeof(szExePath));
	PathRemoveFileSpecW(szExePath);

	size_t n = 0;
	wcstombs_s(&n, buffer, wcSize, szExePath, _TRUNCATE);
#else
	int n = readlink("/proc/self/exe", buffer, sizeof(buffer));
#endif //WIN32
	if (n <= 0) {
		filePath = _exePath;
	} else {
		filePath = buffer;
	}
	return filePath;
}
void setExePath(const string &path){
    _exePath=path;
}
string exeDir(){
	auto path = exePath();
	return path.substr(0, path.find_last_of(DIR_SUFFIX) + 1);
}
string exeName(){
	auto path = exePath();
	return path.substr(path.find_last_of(DIR_SUFFIX) + 1);
}
// string转小写
std::string  strToLower(const std::string &str)
{
    std::string strTmp = str;
    transform(strTmp.begin(), strTmp.end(), strTmp.begin(), towupper);
    return strTmp;
}



#if defined(_WIN32)

static onceToken g_token([]() {
	WORD wVersionRequested = MAKEWORD(2, 2);
	WSADATA wsaData;
	WSAStartup(wVersionRequested, &wsaData);
}, []() {
	WSACleanup();
});
static unordered_map<void *, HANDLE> g_mapFindHandle;
static recursive_mutex g_mtxMap;

int strcasecmp(const char *strA,const char *strB){
	string str1 = strToLower(strA);
	string str2 = strToLower(strB);
	return str1.compare(str2);
}
void sleep(int second) {
	Sleep(1000 * second);
}
void usleep(int micro_seconds) {
	timeval tm;
	tm.tv_sec = micro_seconds / 1000000;
	tm.tv_sec = micro_seconds % (1000000);
	select(0, NULL, NULL, NULL, &tm);
}
int gettimeofday(struct timeval *tp, void *tzp) {
	time_t clock;
	struct tm tm;
	SYSTEMTIME wtm;
	GetLocalTime(&wtm);
	tm.tm_year = wtm.wYear - 1900;
	tm.tm_mon = wtm.wMonth - 1;
	tm.tm_mday = wtm.wDay;
	tm.tm_hour = wtm.wHour;
	tm.tm_min = wtm.wMinute;
	tm.tm_sec = wtm.wSecond;
	tm.tm_isdst = -1;
	clock = mktime(&tm);
	tp->tv_sec = clock;
	tp->tv_usec = wtm.wMilliseconds * 1000;
	return (0);
}

int mkdir(const char *path, int mode) {
	return _mkdir(path);
}
HANDLE getFileHandle(DIR *d) {
	lock_guard<recursive_mutex> lck(g_mtxMap);
	auto it = g_mapFindHandle.find(d);
	if (it == g_mapFindHandle.end()) {
		WarnL << "no such HANDLE!";
		return INVALID_HANDLE_VALUE;
	}
	return it->second;
}
DIR *opendir(const char *name) {
	char namebuf[512];
	sprintf(namebuf, "%s\\*.*", name);

	WIN32_FIND_DATA FindData;
	auto hFind = FindFirstFile(namebuf, &FindData);
	if (hFind == INVALID_HANDLE_VALUE) {
		WarnL << "FindFirstFile failed:" << GetLastError();
		return nullptr;
	}
	DIR *dir = (DIR *)malloc(sizeof(DIR));
	memset(dir, 0, sizeof(DIR));
	dir->dd_fd = 0;   // simulate return  

	lock_guard<recursive_mutex> lck(g_mtxMap);
	g_mapFindHandle[dir] = hFind;
	return dir;
}
struct dirent *readdir(DIR *d) {
	HANDLE hFind = getFileHandle(d);
	if (INVALID_HANDLE_VALUE == hFind) {
		return nullptr;
	}

	WIN32_FIND_DATA FileData;
	//fail or end  
	if (!FindNextFile(hFind, &FileData)) {
		return nullptr;
	}

	struct dirent *dir = (struct dirent *)malloc(sizeof(struct dirent) + sizeof(FileData.cFileName));
	strcpy(dir->d_name, FileData.cFileName);
	dir->d_reclen = strlen(dir->d_name);

	//check there is file or directory  
	if (FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		dir->d_type = 2;
	}
	else {
		dir->d_type = 1;
	}
	if (d->index) {
		//覆盖前释放内存
		free(d->index);
	}
	d->index = dir;
	return dir;
}
int closedir(DIR *d) {
	auto handle = getFileHandle(d);
	if (handle == INVALID_HANDLE_VALUE) {
		//句柄不存在
		return -1;
	}
	//关闭句柄
	FindClose(handle);
	//释放内存
	if (d) {
		if (d->index) {
			free(d->index);
		}
		free(d);
	}
	return 0;
}

int ioctl(int fd, long cmd, u_long *ptr) {
	return ioctlsocket(fd, cmd,ptr);
}
int close(int fd) {
	return closesocket(fd);
}
#endif //WIN32


}  // namespace Util
}  // namespace ZL

