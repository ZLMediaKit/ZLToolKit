//
// Created by xzl on 2019/3/18.
//

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

//字符串缓存
class BufferString : public  Buffer {
public:
    typedef std::shared_ptr<BufferString> Ptr;
    BufferString(const string &data):_data(data) {}
    BufferString(string &&data):_data(std::move(data)){}
    ~BufferString() {}
    char *data() const override {
        return const_cast<char *>(_data.data());
    }
    uint32_t size() const override{
        return _data.size();
    }

    string toString() const override {
        return _data;
    }
private:
    string _data;
};

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
    BufferSock(const Buffer::Ptr &ptr,struct sockaddr *addr = nullptr, int addr_len = 0);
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
