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

#ifndef NETWORK_TCPCLIENT_H
#define NETWORK_TCPCLIENT_H

#include <mutex>
#include <memory>
#include <functional>
#include "Socket.h"
#include "Util/TimeTicker.h"
#include "Util/SSLBox.h"

using namespace std;

namespace toolkit {


//Tcp客户端，接口线程安全的
class TcpClient :
        public std::enable_shared_from_this<TcpClient> ,
        public SocketHelper{
public:
	typedef std::shared_ptr<TcpClient> Ptr;
	TcpClient(const EventPoller::Ptr &poller = nullptr);
	virtual ~TcpClient();
    //开始连接服务器，strUrl可以是域名或ip
    virtual void startConnect(const string &strUrl, uint16_t iPort, float fTimeOutSec = 3);
    //主动断开服务器
    void shutdown(const SockException &ex = SockException(Err_shutdown, "self shutdown")) override ;
    //是否与服务器连接中
    bool alive();
    //设置网卡适配器
    void setNetAdapter(const string &localIp);
protected:
    //连接服务器结果回调
    virtual void onConnect(const SockException &ex) {}
    //收到数据回调
    virtual void onRecv(const Buffer::Ptr &pBuf) {}
    //数据全部发送完毕后回调
    virtual void onFlush() {}
    //被动断开连接回调
    virtual void onErr(const SockException &ex) {}
    //tcp连接成功后每2秒触发一次该事件
    virtual void onManager() {}
private:
	void onSockConnect(const SockException &ex);
private:
    std::shared_ptr<Timer> _managerTimer;
    string _netAdapter = "0.0.0.0";
};

template<typename TcpClientType>
class TcpClientWithSSL: public TcpClientType {
public:
    typedef std::shared_ptr<TcpClientWithSSL> Ptr;

    template<typename ...ArgsType>
    TcpClientWithSSL(ArgsType &&...args):TcpClientType(std::forward<ArgsType>(args)...){}
    virtual ~TcpClientWithSSL(){
        if(_sslBox){
            _sslBox->flush();
        }
    }

    void onRecv(const Buffer::Ptr &pBuf) override{
        if(_sslBox){
            _sslBox->onRecv(pBuf);
        }else{
            TcpClientType::onRecv(pBuf);
        }
    }

    int send(const Buffer::Ptr &buf) override{
        if(_sslBox){
            _sslBox->onSend(buf);
            return buf->size();
        }
        return TcpClientType::send(buf);
    }

    //添加public_onRecv和public_send函数是解决较低版本gcc一个lambad中不能访问protected或private方法的bug
    inline void public_onRecv(const Buffer::Ptr &pBuf){
        TcpClientType::onRecv(pBuf);
    }
    inline void public_send(const Buffer::Ptr &pBuf){
        TcpClientType::send(pBuf);
    }

    void startConnect(const string &strUrl, uint16_t iPort, float fTimeOutSec = 3) override{
        _host = strUrl;
        TcpClientType::startConnect(strUrl,iPort,fTimeOutSec);
    }
protected:
    void onConnect(const SockException &ex)  override {
        if(!ex){
            _sslBox.reset(new SSL_Box(false));
            _sslBox->setOnDecData([this](const Buffer::Ptr &pBuf){
                public_onRecv(pBuf);
            });
            _sslBox->setOnEncData([this](const Buffer::Ptr &pBuf){
                public_send(pBuf);
            });

            if(!isIP(_host.data())){
                //设置ssl域名
                _sslBox->setHost(_host.data());
            }
        }
        TcpClientType::onConnect(ex);
    }
private:
    std::shared_ptr<SSL_Box> _sslBox;
    string _host;
};





} /* namespace toolkit */

#endif /* NETWORK_TCPCLIENT_H */
