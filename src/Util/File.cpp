/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if defined(_WIN32)
#include <io.h>   
#include <direct.h>
#else
#include <dirent.h>
#endif // WIN32

#include <stdlib.h>
#include <sys/stat.h>
#include <string>
#include "File.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/uv_errno.h"

using namespace std;
using namespace ZL::Util;

#if defined(_WIN32)
int mkdir(const char *path, int mode) {
	return _mkdir(path);
}

DIR *opendir(const char *name) {
	char namebuf[512];
	sprintf(namebuf, "%s\\*.*", name);

	WIN32_FIND_DATAA FindData;
    auto hFind = FindFirstFileA(namebuf, &FindData);
	if (hFind == INVALID_HANDLE_VALUE) {
        WarnL << "FindFirstFileA failed:" << get_uv_errmsg();
		return nullptr;
	}
	DIR *dir = (DIR *)malloc(sizeof(DIR));
	memset(dir, 0, sizeof(DIR));
	dir->dd_fd = 0;   // simulate return  
	dir->handle = hFind;
	return dir;
}
struct dirent *readdir(DIR *d) {
	HANDLE hFind = d->handle;
	WIN32_FIND_DATAA FileData;
	//fail or end  
	if (!FindNextFileA(hFind, &FileData)) {
		return nullptr;
	}
	struct dirent *dir = (struct dirent *)malloc(sizeof(struct dirent) + sizeof(FileData.cFileName));
    strcpy(dir->d_name, (char *)FileData.cFileName);
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
		d->index = nullptr;
	}
	d->index = dir;
	return dir;
}
int closedir(DIR *d) {
	if (!d) {
		return -1;
	}
	//关闭句柄
	if (d->handle != INVALID_HANDLE_VALUE) {
		FindClose(d->handle);
		d->handle = INVALID_HANDLE_VALUE;
	}
	//释放内存
	if (d->index) {
		free(d->index);
		d->index = nullptr;
	}
	free(d);
	return 0;
}
#endif // defined(_WIN32)


namespace ZL {
namespace Util {

// 可读普通文件
bool File::isrfile(const char *path) {
	struct stat buf;
	int cc = stat(path, &buf);
	if (cc == -1) {
		//获取文件信息失败
		WarnL << get_uv_errmsg();
		return false;
	}
	//获取文件信息成功
	if ((buf.st_mode & S_IFMT) != S_IFREG) {
		//S_IFMT   0170000    文件类型的位遮罩
		//S_IFREG 0100000     一般文件
		//如果不是一般普通文件
		return false;
	}
#if defined(_WIN32)
	return true;
#else
	if (buf.st_mode & S_IROTH) {
		//S_IROTH 其他用户的读权限
		//如果其他用户可读该文件,那所有用户都可以读该文件
		return true;
	}
	if (buf.st_mode & S_IRGRP) {
		//S_IRGRP 文件所有者同组用户的读权限
		//如果该文件创建者规定同组用户可以读该文件
		/*用来取得执行目前进程有效组识别码。
		 *有效的组识别码用来决定进程执行时组的权限。返回有效的组识别码。
		 */
		unsigned int egid = getegid(); //get-group-id
		if (egid == buf.st_gid) {
			//如果程序启动者与文件创建者是同一组用户
			return true;
		}
	}
	if (buf.st_mode & S_IRUSR) {
		//S_IRUSR 文件所有者的读权限
		//如果该文件创建者规定创建者可以读该文件
		/*	用来取得执行目前进程有效的用户识别码。
		 * 有效的用户识别码用来决定进程执行的权限，
		 * 借由此改变此值，进程可以获得额外的权限。
		 * 倘若执行文件的setID位已被设置，该文件执行时，
		 * 其进程的euid值便会设成该文件所有者的uid。
		 * 例如，执行文件/usr/bin/passwd的权限为-r-s--x--x，
		 * 其s位即为setID（SUID）位，
		 * 而当任何用户在执行passwd时其有效的用户识别码会被设成passwd所有者的uid值，
		 * 即root的uid值（0）。返回有效的用户识别码。
		 */
		unsigned int euid = geteuid(); //get-group-id
		if (euid == buf.st_uid) {
			//如果程序启动者与文件创建者是同一组用户
			return true;
		}
	}
	return false;
#endif // defined(_WIN32)

}

FILE *File::createfile_file(const char *file, const char *mode) {
	std::string path = file;
	std::string dir;
	int index = 1;
	FILE *ret = NULL;
	while (true) {
		index = path.find('/', index) + 1;
		dir = path.substr(0, index);
		if (dir.length() == 0) {
			break;
		}
		if (access(dir.c_str(), 0) == -1) { //access函数是查看是不是存在
			if (mkdir(dir.c_str(), 0777) == -1) {  //如果不存在就用mkdir函数来创建
				WarnL << dir << ":" << get_uv_errmsg();
				return NULL;
			}
		}
	}
	if (path[path.size() - 1] != '/') {
		ret = fopen(file, mode);
	}
	return ret;
}
bool File::createfile_path(const char *file, unsigned int mod) {
	std::string path = file;
	std::string dir;
	int index = 1;
	while (1) {
		index = path.find('/', index) + 1;
		dir = path.substr(0, index);
		if (dir.length() == 0) {
			break;
		}
		if (access(dir.c_str(), 0) == -1) { //access函数是查看是不是存在
			if (mkdir(dir.c_str(), mod) == -1) {  //如果不存在就用mkdir函数来创建
				WarnL << dir << ":" << get_uv_errmsg();
				return false;
			}
		}
	}
	return true;
}

//判断是否为目录
bool File::is_dir(const char *path) {
	struct stat statbuf;
	if (stat(path, &statbuf) == 0) {
		//lstat返回文件的信息，文件信息存放在stat结构中
		if ((statbuf.st_mode & S_IFMT) == S_IFDIR) {
			return true;
		}
#if !defined(_WIN32)
		if (S_ISLNK(statbuf.st_mode)) {
			char realFile[256] = { 0 };
			readlink(path, realFile, sizeof(realFile));
			return File::is_dir(realFile);
		}
#endif // !defined(_WIN32)
	}
	return false;
}

//判断是否为常规文件
bool File::is_file(const char *path) {
	struct stat statbuf;
	if (stat(path, &statbuf) == 0) {
		if ((statbuf.st_mode & S_IFMT) == S_IFREG) {
			return true;
		}
#if !defined(_WIN32)
		if (S_ISLNK(statbuf.st_mode)) {
			char realFile[256] = { 0 };
			readlink(path, realFile, sizeof(realFile));
			return File::is_file(realFile);
		}
#endif // !defined(_WIN32)
	}
	return false;
}

//判断是否是特殊目录
bool File::is_special_dir(const char *path) {
	return strcmp(path, ".") == 0 || strcmp(path, "..") == 0;
}

//生成完整的文件路径
void get_file_path(const char *path, const char *file_name, char *file_path) {
	strcpy(file_path, path);
	if (file_path[strlen(file_path) - 1] != '/') {
		strcat(file_path, "/");
	}
	strcat(file_path, file_name);
}
bool  File::rm_empty_dir(const char *path){
	if(!is_dir(path)){
		string superDir = path;
		superDir = superDir.substr(0, superDir.find_last_of('/') + 1);
		if(superDir == path){
			return false;
		}
		return rm_empty_dir(superDir.data());
	}
	return rmdir(path) == 0;
}
void File::delete_file(const char *path) {
	DIR *dir;
	dirent *dir_info;
	char file_path[PATH_MAX];
	if (is_file(path)) {
		remove(path);
		return;
	}
	if (is_dir(path)) {
		if ((dir = opendir(path)) == NULL) {
			rmdir(path);
			closedir(dir);
			return;
		}
		while ((dir_info = readdir(dir)) != NULL) {
			if (is_special_dir(dir_info->d_name)) {
				continue;
			}
			get_file_path(path, dir_info->d_name, file_path);
			delete_file(file_path);
		}
		rmdir(path);
		closedir(dir);
		return;
	}
	unlink(path);
}

} /* namespace Util */
} /* namespace ZL */
