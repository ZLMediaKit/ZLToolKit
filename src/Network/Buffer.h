/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLTOOLKIT_BUFFER_H
#define ZLTOOLKIT_BUFFER_H

#include <memory>
#include <string>
#include <deque>
#include <mutex>
#include <vector>
#include <atomic>
#include <sstream>
#include <functional>
#include "Util/util.h"
#include "Util/uv_errno.h"
#include "Util/List.h"
#include "Network/sockutil.h"
using namespace std;

namespace toolkit {
//缓存基类
class Buffer : public noncopyable {
public:
    typedef std::shared_ptr<Buffer> Ptr;
    Buffer(){};
    virtual ~Buffer(){};
    //返回数据长度
    virtual char *data() const = 0 ;
    virtual uint32_t size() const = 0;

    virtual string toString() const {
        return string(data(),size());
    }

    virtual uint32_t getCapacity() const{
        return size();
    }
};

template <typename C>
class BufferOffset : public  Buffer {
public:
    typedef std::shared_ptr<BufferOffset> Ptr;

    BufferOffset(C data, int offset = 0, int len = 0) : _data(std::move(data)) {
        setup(offset, len);
    }

    ~BufferOffset() {}

    char *data() const override {
        return const_cast<char *>(_data.data()) + _offset;
    }

    uint32_t size() const override{
        return _size;
    }

    string toString() const override {
        return string(data(),size());
    }

private:
    void setup(int offset = 0,int len = 0){
        _offset = offset;
        _size = len;
        if(_size <= 0 || _size > _data.size()){
            _size = _data.size();
        }
    }

private:
    C _data;
    int _offset;
    int _size;
};

typedef BufferOffset<string> BufferString;

//指针式缓存对象，
class BufferRaw : public Buffer{
public:
    typedef std::shared_ptr<BufferRaw> Ptr;
    BufferRaw(uint32_t capacity = 0) {
        if(capacity){
            setCapacity(capacity);
        }
    }

    BufferRaw(const char *data,int size = 0){
        assign(data,size);
    }

    ~BufferRaw() {
        if(_data){
            delete [] _data;
        }
    }
    //在写入数据时请确保内存是否越界
    char *data() const override {
        return _data;
    }
    //有效数据大小
    uint32_t size() const override{
        return _size;
    }
    //分配内存大小
    void setCapacity(uint32_t capacity){
        if(_data){
            do{
                if(capacity > _capacity){
                    //请求的内存大于当前内存，那么重新分配
                    break;
                }

                if(_capacity < 2 * 1024){
                    //2K以下，不重复开辟内存，直接复用
                    return;
                }

                if(2 * capacity > _capacity){
                    //如果请求的内存大于当前内存的一半，那么也复用
                    return;
                }
            }while(false);

            delete [] _data;
        }
        _data = new char[capacity];
        _capacity = capacity;
    }
    //设置有效数据大小
    void setSize(uint32_t size){
        if(size > _capacity){
            throw std::invalid_argument("Buffer::setSize out of range");
        }
        _size = size;
    }
    //赋值数据
    void assign(const char *data,uint32_t size = 0){
        if(size <=0 ){
            size = strlen(data);
        }
        setCapacity(size + 1);
        memcpy(_data,data,size);
        _data[size] = '\0';
        setSize(size);
    }

    uint32_t getCapacity() const override{
        return _capacity;
    }
private:
    char *_data = nullptr;
    uint32_t _capacity = 0;
    uint32_t _size = 0;
};

class BufferLikeString : public Buffer {
public:
    ~BufferLikeString() override {}

    BufferLikeString() {
        _erase_head = 0;
        _erase_tail  = 0;
    }

    BufferLikeString(string str) {
        _str = std::move(str);
        _erase_head = 0;
        _erase_tail  = 0;
    }

    BufferLikeString& operator= (string str){
        _str = std::move(str);
        _erase_head = 0;
        _erase_tail  = 0;
        return *this;
    }

    BufferLikeString(const char *str) {
        _str = str;
        _erase_head = 0;
        _erase_tail  = 0;
    }

    BufferLikeString& operator= (const char *str){
        _str = str;
        _erase_head = 0;
        _erase_tail  = 0;
        return *this;
    }

    BufferLikeString(BufferLikeString &&that) {
        _str = std::move(that._str);
        _erase_head = that._erase_head;
        _erase_tail = that._erase_tail;
        that._erase_head = 0;
        that._erase_tail = 0;
    }

    BufferLikeString& operator= (BufferLikeString &&that){
        _str = std::move(that._str);
        _erase_head = that._erase_head;
        _erase_tail = that._erase_tail;
        that._erase_head = 0;
        that._erase_tail = 0;
        return *this;
    }

    BufferLikeString(const BufferLikeString &that) {
        _str = that._str;
        _erase_head = that._erase_head;
        _erase_tail = that._erase_tail;
    }

    BufferLikeString& operator= (const BufferLikeString &that){
        _str = that._str;
        _erase_head = that._erase_head;
        _erase_tail = that._erase_tail;
        return *this;
    }

    char* data() const override{
        return (char *)_str.data() + _erase_head;
    }

    uint32_t size() const override{
        return _str.size() - _erase_tail - _erase_head;
    }

    BufferLikeString& erase(string::size_type pos = 0, string::size_type n = string::npos) {
        if (pos == 0) {
            //移除前面的数据
            if (n != string::npos) {
                //移除部分
                if (n > size()) {
                    //移除太多数据了
                    throw std::out_of_range("BufferLikeString::erase out_of_range in head");
                }
                //设置起始便宜量
                _erase_head += n;
                data()[size()] = '\0';
                return *this;
            }
            //移除全部数据
            _erase_head = 0;
            _erase_tail = _str.size();
            data()[0] = '\0';
            return *this;
        }

        if (n == string::npos || pos + n >= size()) {
            //移除末尾所有数据
            if (pos >= size()) {
                //移除太多数据
                throw std::out_of_range("BufferLikeString::erase out_of_range in tail");
            }
            _erase_tail += size() - pos;
            data()[size()] = '\0';
            return *this;
        }

        //移除中间的
        if (pos + n > size()) {
            //超过长度限制
            throw std::out_of_range("BufferLikeString::erase out_of_range in middle");
        }
        _str.erase(_erase_head + pos, n);
        return *this;
    }

    BufferLikeString& append(const BufferLikeString &str){
        return append(str.data(), str.size());
    }

    BufferLikeString& append(const string &str){
        return append(str.data(), str.size());
    }

    BufferLikeString& append(const char *data, int len = 0){
        if(_erase_head > _str.capacity() / 2){
            moveData();
        }
        if (_erase_tail == 0) {
            if (len <= 0) {
                _str.append(data);
            } else {
                _str.append(data, len);
            }
            return *this;
        }
        if (len <= 0) {
            _str.insert(_erase_head + size(), data);
        } else {
            _str.insert(_erase_head + size(), data, len);
        }
        return *this;
    }

    void push_back(char c){
        if(_erase_tail == 0){
            _str.push_back(c);
            return;
        }
        data()[size()] = c;
        --_erase_tail;
        data()[size()] = '\0';
    }

    BufferLikeString& insert(string::size_type pos, const char* s, string::size_type n){
        _str.insert(_erase_head + pos, s, n);
        return *this;
    }

    BufferLikeString& assign(const char *data, int len = 0) {
        if (data >= _str.data() && data < _str.data() + _str.size()) {
            _erase_head = data - _str.data();
            len = len ? len : strlen(data);
            if (data + len > _str.data() + _str.size()) {
                throw std::out_of_range("BufferLikeString::assign out_of_range");
            }
            _erase_tail = _str.data() + _str.size() - (data + len);
            return *this;
        }
        if (len <= 0) {
            _str.assign(data);
        } else {
            _str.assign(data, len);
        }
        _erase_head = 0;
        _erase_tail = 0;
        return *this;
    }

    void clear() {
        _erase_head = 0;
        _erase_tail = 0;
        _str.clear();
    }

    char& operator[](string::size_type pos){
        if (pos >= size()) {
            throw std::out_of_range("BufferLikeString::operator[] out_of_range");
        }
        return data()[pos];
    }

    const char& operator[](string::size_type pos) const{
        return (*const_cast<BufferLikeString *>(this))[pos];
    }

    string::size_type capacity() const{
        return _str.capacity();
    }

    void reserve(string::size_type size){
        _str.reserve(size);
    }

    bool empty() const{
        return size() <= 0;
    }

    string substr(string::size_type pos, string::size_type n = string::npos) const{
        if (n == string::npos) {
            //获取末尾所有的
            if (pos >= size()) {
                throw std::out_of_range("BufferLikeString::substr out_of_range");
            }
            return _str.substr(_erase_head + pos, size() - pos);
        }

        //获取部分
        if (pos + n > size()) {
            throw std::out_of_range("BufferLikeString::substr out_of_range");
        }
        return _str.substr(_erase_head + pos, n);
    }

private:
    void moveData(){
        if (_erase_head) {
            _str.erase(0, _erase_head);
            _erase_head = 0;
        }
    }

private:
    uint32_t _erase_head;
    uint32_t _erase_tail;
    string _str;
};

#if defined(_WIN32)
struct iovec {
    void *   iov_base;	/* [XSI] Base address of I/O memory region */
    int	 iov_len;	/* [XSI] Size of region iov_base points to */
};
struct msghdr {
    void		*msg_name;	/* [XSI] optional address */
    int			msg_namelen;	/* [XSI] size of address */
    struct		iovec *msg_iov;	/* [XSI] scatter/gather array */
    int			msg_iovlen;	/* [XSI] # elements in msg_iov */
    void		*msg_control;	/* [XSI] ancillary data, see below */
    int			msg_controllen;	/* [XSI] ancillary data buffer len */
    int			msg_flags;	/* [XSI] flags on received message */
};
#else
#include <sys/uio.h>
#include <limits.h>
#endif

#if !defined(IOV_MAX)
#define IOV_MAX 1024
#endif

class BufferList;
class BufferSock : public Buffer{
public:
    typedef std::shared_ptr<BufferSock> Ptr;
    friend class BufferList;
    BufferSock(Buffer::Ptr ptr,struct sockaddr *addr = nullptr, int addr_len = 0);
    ~BufferSock();
    char *data() const override ;
    uint32_t size() const override;
private:
    Buffer::Ptr _buffer;
    struct sockaddr *_addr = nullptr;
    int  _addr_len = 0;
};

class BufferList : public noncopyable {
public:
    typedef std::shared_ptr<BufferList> Ptr;
    BufferList(List<Buffer::Ptr> &list);
    ~BufferList(){}
    bool empty();
    int count();
    int send(int fd,int flags,bool udp);
private:
    void reOffset(int n);
    int send_l(int fd,int flags,bool udp);
private:
    vector<struct iovec> _iovec;
    int _iovec_off = 0;
    int _remainSize = 0;
    List<Buffer::Ptr> _pkt_list;
};

}//namespace toolkit
#endif //ZLTOOLKIT_BUFFER_H
