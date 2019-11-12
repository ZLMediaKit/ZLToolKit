/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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

#ifndef SRC_UTIL_FILE_H_
#define SRC_UTIL_FILE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "util.h"
#include <functional>

using namespace std;
#if defined(__linux__)
#include <limits.h>
#endif

#if defined(_WIN32)
#include <WinSock2.h>   
#pragma comment (lib,"WS2_32")
#endif // WIN32

#if defined(_WIN32)
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif // !PATH_MAX

struct dirent{
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
	HANDLE handle;
	struct dirent *index;
} DIR;
# define __dirfd(dp)    ((dp)->dd_fd)

int mkdir(const char *path, int mode);
DIR *opendir(const char *);
int closedir(DIR *);
struct dirent *readdir(DIR *);

#endif // defined(_WIN32)

namespace toolkit {

class File {
public:
	//新建文件，目录文件夹自动生成
	static bool createfile_path(const char *file, unsigned int mod);
	static FILE *createfile_file(const char *file,const char *mode);
	//判断是否为目录
	static bool is_dir(const char *path) ;
	//判断是否为常规文件
	static bool is_file(const char *path) ;
	//判断是否是特殊目录（. or ..）
	static bool is_special_dir(const char *path);
	//删除目录或文件
	static void delete_file(const char *path) ;

	/**
	 * 加载文件内容至string
	 * @param path 加载的文件路径
	 * @return 文件内容
	 */
	static string loadFile(const char *path);

	/**
	 * 保存内容至文件
	 * @param data 文件内容
	 * @param path 保存的文件路径
	 * @return 是否保存成功
	 */
	static bool saveFile(const string &data,const char *path);

	/**
	 * 获取父文件夹
	 * @param path 路径
	 * @return 文件夹
	 */
	static string parentDir(const string &path);

    /**
     * 替换"../"，获取绝对路径
     * @param path 相对路径，里面可能包含 "../"
     * @param currentPath 当前目录
	 * @param canAccessParent 能否访问父目录之外的目录
     * @return 替换"../"之后的路径
     */
	static string absolutePath(const string &path, const string &currentPath,bool canAccessParent = false);

	/**
	 * 遍历文件夹下的所有文件
	 * @param path 文件夹路径
	 * @param cb 回调对象 ，path为绝对路径，isDir为该路径是否为文件夹，返回true代表继续扫描，否则中断
	 * @param enterSubdirectory 是否进入子目录扫描
	 */
	static void scanDir(const string &path,const function<bool(const string &path,bool isDir)> &cb, bool enterSubdirectory = false);
private:
	File();
	~File();
};

} /* namespace toolkit */

#endif /* SRC_UTIL_FILE_H_ */
