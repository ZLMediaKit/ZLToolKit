/*
 * File.h
 *
 *  Created on: 2016年9月22日
 *      Author: xzl
 */

#ifndef SRC_UTIL_FILE_H_
#define SRC_UTIL_FILE_H_

#include <stdio.h>

namespace ZL {
namespace Util {

class File {
public:
	// 可读普通文件
	static bool isrfile(const char *path) ;
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
	static bool rm_empty_dir(const char *path);
private:
	File();
	virtual ~File();
};

} /* namespace Util */
} /* namespace ZL */

#endif /* SRC_UTIL_FILE_H_ */
