/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
*  Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to
* deal in the Software without restriction, including without limitation the
* rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
* sell copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
*/
#include "uv_errno.h"
#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
#define FD_SETSIZE 1024 //修改默认64为1024路
#include <WinSock2.h>
#include <Windows.h>
#else
#include <errno.h>
#endif // defined(_WIN32)


static const char* uv__unknown_err_code(int err) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "Unknown system error %d", err);
    return buf;
}

#define UV_ERR_NAME_GEN(name, _) case UV_ ## name: return #name;
const char* uv_err_name(int err) {
    switch (err) {
        UV_ERRNO_MAP(UV_ERR_NAME_GEN)
    }
    return uv__unknown_err_code(err);
}
#undef UV_ERR_NAME_GEN


#define UV_STRERROR_GEN(name, msg) case UV_ ## name: return msg;
const char* uv_strerror(int err) {
    switch (err) {
        UV_ERRNO_MAP(UV_STRERROR_GEN)
    }
    return uv__unknown_err_code(err);
}
#undef UV_STRERROR_GEN


int uv_translate_posix_error(int err) {
    if (err <= 0) {
        return err;
    }
    switch (err) {
        //为了兼容windows/unix平台，信号EINPROGRESS ，EAGAIN，EWOULDBLOCK，ENOBUFS 全部统一成EAGAIN处理
    case ENOBUFS://在mac系统下实测发现会有此信号发生
    case EINPROGRESS:
    case EWOULDBLOCK: err = EAGAIN; break;
    default:
        break;
    }
    return -err;
}

int get_uv_error(bool netErr) {
#if defined(_WIN32)
    auto errCode = netErr ? WSAGetLastError() : GetLastError();
    switch (errCode) {
    case ERROR_NOACCESS:                    return UV_EACCES;
    case WSAEACCES:                         return UV_EACCES;
#if defined(ERROR_ELEVATION_REQUIRED)
    case ERROR_ELEVATION_REQUIRED:          return UV_EACCES;
#endif //ERROR_ELEVATION_REQUIRED
    case ERROR_ADDRESS_ALREADY_ASSOCIATED:  return UV_EADDRINUSE;
    case WSAEADDRINUSE:                     return UV_EADDRINUSE;
    case WSAEADDRNOTAVAIL:                  return UV_EADDRNOTAVAIL;
    case WSAEAFNOSUPPORT:                   return UV_EAFNOSUPPORT;
    case WSAEWOULDBLOCK:                    return UV_EAGAIN;
    case WSAEALREADY:                       return UV_EALREADY;
    case ERROR_INVALID_FLAGS:               return UV_EBADF;
    case ERROR_INVALID_HANDLE:              return UV_EBADF;
    case ERROR_LOCK_VIOLATION:              return UV_EBUSY;
    case ERROR_PIPE_BUSY:                   return UV_EBUSY;
    case ERROR_SHARING_VIOLATION:           return UV_EBUSY;
    case ERROR_OPERATION_ABORTED:           return UV_ECANCELED;
    case WSAEINTR:                          return UV_ECANCELED;
    case ERROR_NO_UNICODE_TRANSLATION:      return UV_ECHARSET;
    case ERROR_CONNECTION_ABORTED:          return UV_ECONNABORTED;
    case WSAECONNABORTED:                   return UV_ECONNABORTED;
    case ERROR_CONNECTION_REFUSED:          return UV_ECONNREFUSED;
    case WSAECONNREFUSED:                   return UV_ECONNREFUSED;
    case ERROR_NETNAME_DELETED:             return UV_ECONNRESET;
    case WSAECONNRESET:                     return UV_ECONNRESET;
    case ERROR_ALREADY_EXISTS:              return UV_EEXIST;
    case ERROR_FILE_EXISTS:                 return UV_EEXIST;
    case ERROR_BUFFER_OVERFLOW:             return UV_EFAULT;
    case WSAEFAULT:                         return UV_EFAULT;
    case ERROR_HOST_UNREACHABLE:            return UV_EHOSTUNREACH;
    case WSAEHOSTUNREACH:                   return UV_EHOSTUNREACH;
    case ERROR_INSUFFICIENT_BUFFER:         return UV_EINVAL;
    case ERROR_INVALID_DATA:                return UV_EINVAL;
    case ERROR_INVALID_PARAMETER:           return UV_EINVAL;
#if defined(ERROR_SYMLINK_NOT_SUPPORTED)
    case ERROR_SYMLINK_NOT_SUPPORTED:       return UV_EINVAL;
#endif //ERROR_SYMLINK_NOT_SUPPORTED
    case WSAEINVAL:                         return UV_EINVAL;
    case WSAEPFNOSUPPORT:                   return UV_EINVAL;
    case WSAESOCKTNOSUPPORT:                return UV_EINVAL;
    case ERROR_BEGINNING_OF_MEDIA:          return UV_EIO;
    case ERROR_BUS_RESET:                   return UV_EIO;
    case ERROR_CRC:                         return UV_EIO;
    case ERROR_DEVICE_DOOR_OPEN:            return UV_EIO;
    case ERROR_DEVICE_REQUIRES_CLEANING:    return UV_EIO;
    case ERROR_DISK_CORRUPT:                return UV_EIO;
    case ERROR_EOM_OVERFLOW:                return UV_EIO;
    case ERROR_FILEMARK_DETECTED:           return UV_EIO;
    case ERROR_GEN_FAILURE:                 return UV_EIO;
    case ERROR_INVALID_BLOCK_LENGTH:        return UV_EIO;
    case ERROR_IO_DEVICE:                   return UV_EIO;
    case ERROR_NO_DATA_DETECTED:            return UV_EIO;
    case ERROR_NO_SIGNAL_SENT:              return UV_EIO;
    case ERROR_OPEN_FAILED:                 return UV_EIO;
    case ERROR_SETMARK_DETECTED:            return UV_EIO;
    case ERROR_SIGNAL_REFUSED:              return UV_EIO;
    case WSAEISCONN:                        return UV_EISCONN;
    case ERROR_CANT_RESOLVE_FILENAME:       return UV_ELOOP;
    case ERROR_TOO_MANY_OPEN_FILES:         return UV_EMFILE;
    case WSAEMFILE:                         return UV_EMFILE;
    case WSAEMSGSIZE:                       return UV_EMSGSIZE;
    case ERROR_FILENAME_EXCED_RANGE:        return UV_ENAMETOOLONG;
    case ERROR_NETWORK_UNREACHABLE:         return UV_ENETUNREACH;
    case WSAENETUNREACH:                    return UV_ENETUNREACH;
    case WSAENOBUFS:                        return UV_ENOBUFS;
    case ERROR_BAD_PATHNAME:                return UV_ENOENT;
    case ERROR_DIRECTORY:                   return UV_ENOENT;
    case ERROR_FILE_NOT_FOUND:              return UV_ENOENT;
    case ERROR_INVALID_NAME:                return UV_ENOENT;
    case ERROR_INVALID_DRIVE:               return UV_ENOENT;
    case ERROR_INVALID_REPARSE_DATA:        return UV_ENOENT;
    case ERROR_MOD_NOT_FOUND:               return UV_ENOENT;
    case ERROR_PATH_NOT_FOUND:              return UV_ENOENT;
    case WSAHOST_NOT_FOUND:                 return UV_ENOENT;
    case WSANO_DATA:                        return UV_ENOENT;
    case ERROR_NOT_ENOUGH_MEMORY:           return UV_ENOMEM;
    case ERROR_OUTOFMEMORY:                 return UV_ENOMEM;
    case ERROR_CANNOT_MAKE:                 return UV_ENOSPC;
    case ERROR_DISK_FULL:                   return UV_ENOSPC;
    case ERROR_EA_TABLE_FULL:               return UV_ENOSPC;
    case ERROR_END_OF_MEDIA:                return UV_ENOSPC;
    case ERROR_HANDLE_DISK_FULL:            return UV_ENOSPC;
    case ERROR_NOT_CONNECTED:               return UV_ENOTCONN;
    case WSAENOTCONN:                       return UV_ENOTCONN;
    case ERROR_DIR_NOT_EMPTY:               return UV_ENOTEMPTY;
    case WSAENOTSOCK:                       return UV_ENOTSOCK;
    case ERROR_NOT_SUPPORTED:               return UV_ENOTSUP;
    case ERROR_BROKEN_PIPE:                 return UV_EOF;
    case ERROR_ACCESS_DENIED:               return UV_EPERM;
    case ERROR_PRIVILEGE_NOT_HELD:          return UV_EPERM;
    case ERROR_BAD_PIPE:                    return UV_EPIPE;
    case ERROR_NO_DATA:                     return UV_EPIPE;
    case ERROR_PIPE_NOT_CONNECTED:          return UV_EPIPE;
    case WSAESHUTDOWN:                      return UV_EPIPE;
    case WSAEPROTONOSUPPORT:                return UV_EPROTONOSUPPORT;
    case ERROR_WRITE_PROTECT:               return UV_EROFS;
    case ERROR_SEM_TIMEOUT:                 return UV_ETIMEDOUT;
    case WSAETIMEDOUT:                      return UV_ETIMEDOUT;
    case ERROR_NOT_SAME_DEVICE:             return UV_EXDEV;
    case ERROR_INVALID_FUNCTION:            return UV_EISDIR;
    case ERROR_META_EXPANSION_TOO_LONG:     return UV_E2BIG;
    default:                                return UV_UNKNOWN;
    }
#else
    return uv_translate_posix_error(errno);
#endif // defined(_WIN32)
}


const char* get_uv_errmsg(bool netErr) {
    return uv_strerror(get_uv_error(netErr));
}
