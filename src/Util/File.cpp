/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
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
#include "Util/onceToken.h"

using namespace std;
using namespace toolkit;

#if !defined(_WIN32)
    #define	_unlink	unlink
    #define	_rmdir	rmdir
    #define	_access	access
#endif

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


namespace toolkit {

FILE *File::create_file(const char *file, const char *mode) {
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
        if (_access(dir.c_str(), 0) == -1) { //access函数是查看是不是存在
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
bool File::create_path(const char *file, unsigned int mod) {
    std::string path = file;
    std::string dir;
    int index = 1;
    while (1) {
        index = path.find('/', index) + 1;
        dir = path.substr(0, index);
        if (dir.length() == 0) {
            break;
        }
        if (_access(dir.c_str(), 0) == -1) { //access函数是查看是不是存在
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
            _rmdir(path);
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
        _rmdir(path);
        closedir(dir);
        return;
    }
    _unlink(path);
}

string File::loadFile(const char *path) {
    FILE *fp = fopen(path,"rb");
    if(!fp){
        return "";
    }
    fseek(fp,0,SEEK_END);
    auto len = ftell(fp);
    fseek(fp,0,SEEK_SET);
    string str(len,'\0');
    fread((char *)str.data(),str.size(),1,fp);
    fclose(fp);
    return str;
}

bool File::saveFile(const string &data, const char *path) {
    FILE *fp = fopen(path,"wb");
    if(!fp){
        return false;
    }
    fwrite(data.data(),data.size(),1,fp);
    fclose(fp);
    return true;
}

string File::parentDir(const string &path) {
    auto parent_dir = path;
    if(parent_dir.back() == '/'){
        parent_dir.pop_back();
    }
    auto pos = parent_dir.rfind('/');
    if(pos != string::npos){
        parent_dir = parent_dir.substr(0,pos + 1);
    }
    return std::move(parent_dir);
}

string File::absolutePath(const string &path,const string &currentPath_in,bool canAccessParent) {
    string currentPath = currentPath_in;
    if(!currentPath.empty()){
        //当前目录不为空
        if(currentPath.front() == '.'){
            //如果当前目录是相对路径，那么先转换成绝对路径
            currentPath = absolutePath(currentPath_in,exeDir(),true);
        }
    } else{
        currentPath = exeDir();
    }

    if(path.empty()){
        //相对路径为空，那么返回当前目录
        return currentPath;
    }

    if(currentPath.back() != '/'){
        //确保当前目录最后字节为'/'
        currentPath.push_back('/');
    }

    auto dir_vec = split(path,"/");
    for(auto &dir : dir_vec){
        if(dir.empty() || dir == "."){
            //忽略空或本文件夹
            continue;
        }
        if(dir == ".."){
            //访问上级目录
            if(!canAccessParent && currentPath.size() <= currentPath_in.size()){
                //不能访问根目录之外的目录
                return "";
            }
            currentPath = parentDir(currentPath);
            continue;
        }
        currentPath.append(dir);
        currentPath.append("/");
    }

    if(path.back() != '/' && currentPath.back() == '/'){
        //在路径是文件的情况下，防止转换成目录
        currentPath.pop_back();
    }
    return currentPath;
}

void File::scanDir(const string &path_in, const function<bool(const string &path, bool isDir)> &cb, bool enterSubdirectory) {
    string path = path_in;
    if(path.back() == '/'){
        path.pop_back();
    }

    DIR *pDir;
    dirent *pDirent;
    if ((pDir = opendir(path.data())) == NULL) {
        //文件夹无效
        return;
    }
    while ((pDirent = readdir(pDir)) != NULL) {
        if (is_special_dir(pDirent->d_name)) {
            continue;
        }
        if(pDirent->d_name[0] == '.'){
            //隐藏的文件
            continue;
        }
        string strAbsolutePath = path + "/" + pDirent->d_name;
        bool isDir = is_dir(strAbsolutePath.data());
        if(!cb(strAbsolutePath,isDir)){
            //不再继续扫描
            break;
        }

        if(isDir && enterSubdirectory){
            //如果是文件夹并且扫描子文件夹，那么递归扫描
            scanDir(strAbsolutePath,cb,enterSubdirectory);
        }
    }
    closedir(pDir);
}

} /* namespace toolkit */
