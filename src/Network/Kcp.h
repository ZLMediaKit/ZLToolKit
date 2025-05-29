/*
 * Copyright (c) 2021 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TOOLKIT_NETWORK_KCP_H
#define TOOLKIT_NETWORK_KCP_H

#include "Network/Buffer.h"
#include "Network/sockutil.h"
#include "Poller/EventPoller.h"
#include "Poller/Timer.h"
#include "Util/TimeTicker.h"

namespace toolkit {

class KcpHeader {
public:
    static const size_t HEADER_SIZE = 24;

    enum class Cmd : uint8_t {
        CMD_PUSH = 81,  // cmd: push data
        CMD_ACK  = 82,  // cmd: ack
        CMD_WASK = 83,  // cmd: window probe (ask)
        CMD_WINS = 84,  // cmd: window size (tell)
    };

    uint32_t _conv;     // 会话ID,用于标识一个会话
    Cmd      _cmd;      // 命令字段,用于标识数据包类型
    uint8_t  _frg = 0;  // 分片序号,用于消息分片,0表示最后一片
    uint16_t _wnd;      // 接受窗口大小
    uint32_t _ts;       // 时间戳,2^32ms,约49.7天会溢出一次
    uint32_t _sn;       // 序列号
    uint32_t _una;      // 待接收的第一个未确认包序号
    uint32_t _len = 0;  // payload部分数据长度(不包含头长度)

public:

    // Getters for KcpHeader members
    uint32_t getConv() const { return _conv; }
    Cmd getCmd() const { return _cmd; }
    uint8_t getFrg() const { return _frg; }
    uint16_t getWnd() const { return _wnd; }
    uint32_t getTs() const { return _ts; }
    uint32_t getSn() const { return _sn; }
    uint32_t getUna() const { return _una; }
    uint32_t getLen() const { return _len; }

    // Setters for KcpHeader members
    void setConv(uint32_t conv) { _conv = conv; }
    void setCmd(Cmd cmd) { _cmd = cmd; }
    void setFrg(uint8_t frg) { _frg = frg; }
    void setWnd(uint16_t wnd) { _wnd = wnd; }
    void setTs(uint32_t ts) { _ts = ts; }
    void setSn(uint32_t sn) { _sn = sn; }
    void setUna(uint32_t una) { _una = una; }
    void setLen(uint32_t len) { _len = len; }

    uint32_t getPacketSize() const { return _len + HEADER_SIZE; }
    bool loadHeaderFromData(const char *data, size_t len);
};

class KcpPacket : public KcpHeader, public toolkit::BufferRaw {
public:
    using Ptr = std::shared_ptr<KcpPacket>;

    static KcpPacket::Ptr parse(const char* data, size_t len);

    KcpPacket() {};
    KcpPacket(uint32_t conv, Cmd cmd, size_t payloadSize) {
        setConv(conv);
        setCmd(cmd);
        setPayLoadSize(payloadSize);
    };

    KcpPacket(size_t payloadSize) {
        setPayLoadSize(payloadSize);
    }

    virtual ~KcpPacket();

    char *getPayloadData() {
        return data() + HEADER_SIZE;
    };

    uint32_t getResendts() const { return _resendts; }
    uint32_t getRto() const { return _rto; }
    uint32_t getFastack() const { return _fastack; }
    uint32_t getXmit() const { return _xmit; }

    void setResendts(uint32_t resendts) { _resendts = resendts; }
    void setRto(uint32_t rto) {_rto = rto; }
    void setFastack(uint32_t fastack) { _fastack = fastack; }
    void setXmit(uint32_t xmit) { _xmit = xmit; }

    void setPayLoadSize(size_t len) {
        setCapacity(len + HEADER_SIZE + 1);
        setSize(len + HEADER_SIZE);
        setLen(len);
    }

protected:
    bool loadFromData(const char *data, size_t len);

private:
    uint32_t _resendts; // 重传超时时间戳,表示该数据包下次重传的时间戳
    uint32_t _rto;      // 超时重传时间，表示数据包在多长时间没收到ACK就重传,会基于rtt动态调整
    uint32_t _fastack;  // 快速确认计数器
    uint32_t _xmit;     // 传输次数,用于统计重传次数
};

//数据包
class KcpDataPacket : public KcpPacket {
public:
    KcpDataPacket(uint32_t conv, size_t payloadSize)
        : KcpPacket(conv, KcpHeader::Cmd::CMD_WASK, payloadSize) {
    }
};

//ACK包
class KcpAckPacket : public KcpPacket {
public:
    KcpAckPacket(uint32_t conv) 
        : KcpPacket(conv, KcpHeader::Cmd::CMD_ACK, 0) {
    }
};

//探测窗口大小包
class KcpProbePacket : public KcpPacket {
public:
    KcpProbePacket(uint32_t conv)
        : KcpPacket(conv, KcpHeader::Cmd::CMD_WASK, 0) {
    }

};

//告知窗口大小包
class KcpTellPacket : public KcpPacket {
public:
    KcpTellPacket(uint32_t conv)
        : KcpPacket(conv, KcpHeader::Cmd::CMD_WINS, 0) {
    }
};

class KcpTransport : public std::enable_shared_from_this<KcpTransport> {
public:
    using Ptr = std::shared_ptr<KcpTransport>;
    enum State {
        STATE_INIT = 0,      // 初始化状态，KCP实例刚创建
        STATE_TIMEOUT = 1,    // 连接超时，dead_link计数超过阈值
    };

    enum DelayMode {
        DELAY_MODE_NORMAL   = 0,    // 正常模式, 每次重发rto翻倍,往外增加12.5%的最小rto
        DELAY_MODE_FAST     = 1,    // 快速模式, 每次重发rto增加当前包rto的一半,不额外增加延时
        DELAY_MODE_NO_DELAY = 2,    // 极速模式, 每次重发rto增加基础rto的一半,不额外增加延时
    };

    static const uint32_t IKCP_ASK_SEND = 1;		// need to send IKCP_CMD_WASK
    static const uint32_t IKCP_ASK_TELL = 2;		// need to send IKCP_CMD_WINS

    static const uint32_t IKCP_RTO_NDL = 30;		// no delay min rto
    static const uint32_t IKCP_RTO_MIN = 100;		// normal min rto
    static const uint32_t IKCP_RTO_DEF = 200;
    static const uint32_t IKCP_RTO_MAX = 60000;

    static const uint32_t IKCP_WND_SND = 32;
    static const uint32_t IKCP_WND_RCV = 128;       // must >= max fragment size
    static const uint32_t IKCP_MTU_DEF = 1400;
    static const uint32_t IKCP_ACK_FAST	= 3;
    static const uint32_t IKCP_INTERVAL	= 100;
    static const uint32_t IKCP_THRESH_INIT = 2;
    static const uint32_t IKCP_THRESH_MIN = 2;
    static const uint32_t IKCP_PROBE_INIT = 7000;		// 7 secs to probe window size
    static const uint32_t IKCP_PROBE_LIMIT = 120000;	// up to 120 secs to probe window
 
    using onReadCB = std::function<void(const Buffer::Ptr &buf)>;
    using onWriteCB = std::function<void(const Buffer::Ptr &buf)>;

    KcpTransport(bool serverMode);
    virtual ~KcpTransport();

    void setOnRead(onReadCB cb) { _on_read = cb; }
    void setOnWrite(onWriteCB cb) { _on_write = cb; }

    // 应用层将数据放到发送队列中
    ssize_t send(const Buffer::Ptr &buf, bool flush = false);

    // 应用层将socket层接收到的数据输入
    void input(const Buffer::Ptr &buf);

    // change MTU size, default is 1400
    void setMtu(int mtu);

    void setInterval(int intervoal);

    // set maximum window size: sndwnd=32, rcvwnd=32 by default
    void setWndSize(int sndwnd, int rcvwnd);

    // fastest: ikcp_nodelay(kcp, 1, 20, 2, 1)
    // nodelay: 0:disable(default), 1:enable
    // interval: internal update timer interval in millisec, default is 100ms 
    // resend: 0:disable fast resend(default), 1:enable fast resend
    // nc: 0:normal congestion control(default), 1:disable congestion control
    void setDelayMode(DelayMode delay_mode);

    //设置快速重传的阈值
    void setFastResend(int resend);

    //设置快速重传保守模式
    //默认保守模式
    void setFastackConserve(bool flag);

    //设置是否开启拥塞控制
    void setNoCwnd(bool flag);

    //设置是否开启流传输模式
    void setStreamMode(bool flag);

protected:

    void onWrite(const Buffer::Ptr &buf) {
        if (_on_write) {
            _on_write(buf);
        }
    }

    void onRead(const Buffer::Ptr &buf) {
        if (_on_read) {
            _on_read(buf);
        }
    }

    void startTimer();

    //处理收到的数据,rcv_buf中有新数据时调用
    void onData();

    //测量rcv_queue 下一个可以提取的包的长度
    int peeksize();

    void handleAnyPacket(KcpPacket::Ptr packet);
    void handleCmdAck(KcpPacket::Ptr packet, uint32_t current);
    void handleCmdPush(KcpPacket::Ptr packet);

    // move available data from rcv_buf -> rcv_queue
    void sortRecvBuf();
    void sortSendQueue();
    //流模式,合并发送包
    size_t mergeSendQueue(const char *buffer, size_t len);

    // 将发送队列的数据真正发送出去
    void update();
    void sendSendQueue();
    void sendAckList();
    void sendProbePacket();
    void sendPacket(Buffer::Ptr pkt, bool flush = false);
    void flushPool();

    //将发送缓存中对端已经确认的数据包丢弃
    //UNA模式,指定序列之前的包都已经确认,可以Drop
    void dropCacheByUna(uint32_t una);

    //将发送缓存中对端已经确认的数据包丢弃
    //ACK模式,仅指定序列的包被确认
    void dropCacheByAck(uint32_t sn);

    //更新rtt
    void updateRtt(int32_t rtt);

    //更新发送cache中packet的Faskack计数
    void updateFastAck(uint32_t sn, uint32_t ts);

    //扩大拥塞窗口
    void increaseCwnd();

    //缩小拥塞窗口
    void decreaseCwnd(bool change, bool lost);

    // get how many packet is waiting to be sent
    int getWaitSnd();

    int getRcvWndUnused();

private:
    onReadCB _on_read = nullptr;
    onWriteCB _on_write = nullptr;

    bool _server_mode;
    bool _conv_init = false;

    EventPoller::Ptr _poller;
    Timer::Ptr _timer;
    //刷新计时器
    Ticker _alive_ticker;

    bool _fastack_conserve = false;  //快速重传保守模式

    uint32_t _conv;    // 会话ID,用于标识一个会话
    uint32_t _mtu  = IKCP_MTU_DEF;     // 最大传输单元,默认1400
    uint32_t _mss  = IKCP_MTU_DEF - KcpPacket::HEADER_SIZE;     // 最大分片大小,由MTU计算得到
    State    _state = STATE_INIT;   // 连接状态

    uint32_t _interval = IKCP_INTERVAL;  //内部flush的率先哪个间隔
    uint32_t _ts_flush = IKCP_INTERVAL;  //下次需要flush的时间戳

    uint _fastresend = 0;  //快速重传触发阈值,当packet的_fastack超过该值时,触发快速重传
    int _fastlimit = 5;   //快速重传限制，限制触发快速重传的最大次数,防止过度重传

    uint32_t _xmit = 0;      //重传次数计数器
    uint32_t _dead_link = 20; //最大重传次数,当某个包的重传次数超过该值时，认为链路断开

    uint32_t _snd_una = 0; //发送缓冲区中第一个未确认的包序号
    uint32_t _snd_nxt = 0; //下一个待分配的序号
    uint32_t _rcv_nxt = 0; //接收队列中待接收的下一个包序号

    uint32_t _ts_recent = 0; //最近一次收到数据包的时间戳
    uint32_t _ts_lastack = 0;//最近一次发送ACK的时间戳

    //rtt
    int32_t _rx_rttval = 0;  //RTT方差
    int32_t _rx_srtt = 0;    //RTT(平滑后)
    int32_t _rx_rto = IKCP_RTO_DEF; //重传超时时间(会基于rtt和rtt方差动态调整)
    int32_t _rx_minrto = IKCP_RTO_MIN; //最小重传超时时间,防止RTO过小

    //for 拥塞窗口控制
    uint32_t _snd_wnd = IKCP_WND_SND; //发送队列窗口,用于限制发送速率,用户配置(单位分片数量)
    uint32_t _rcv_wnd = IKCP_WND_RCV; //接收队列窗口,用于限制接收速率,用户配置(单位分片数量)
    uint32_t _rmt_wnd = IKCP_WND_RCV; //对端接收缓存拥塞窗口,对端通告(单位分片数量)
    uint32_t _cwnd = 0;  //发送缓存拥塞窗口大小,算法动态调整(单位分片数量)
    uint32_t _incr = 0;  //拥塞窗口增量,用于拥塞控制算法中动态窗口大小(单位字节)
    uint32_t _ssthresh = IKCP_THRESH_INIT;  //慢启动阈值

    uint32_t _probe = 0;   //探测标志,用于探测对端窗口大小
    uint32_t _ts_probe = 0; //探测时间戳,记录发送窗口探测包的时间戳
    uint32_t _probe_wait = 0;//探测等待时间, 控制探测包发送的时间间隔

    DelayMode _delay_mode = DELAY_MODE_NORMAL;
    int _nocwnd = false; //是否禁用拥塞控制
    bool _stream = false; //是否开启流传输模式

    //传输链路: userdata->_snd_queue->_snd_buf->网络发送
    //_snd_queue:无限制
    //_snd_buf: min(_snd_wnd, _rmt_wnd, _cwnd)
    //传输链路: 网络接收->_rcv_buf->_snd_queue->userdata
    //_rcv_buf:无限制,乱序数据暂存
    //_snd_queue: _rcv_wnd
    std::list<KcpDataPacket::Ptr> _snd_queue; //发送队列,还未进入发送窗口
    std::list<KcpDataPacket::Ptr> _rcv_queue; //接收队列,已经接收完全的包等待交给应用层
    std::list<KcpDataPacket::Ptr> _snd_buf;   //发送缓存,已经进入发送窗口,用于重传
    std::list<KcpDataPacket::Ptr> _rcv_buf;   //接收缓存,已经接受，但是因为乱序丢包等还不能交给应用层
    //待发送的ACK列表
    std::deque<std::pair<uint32_t /*sn*/, uint32_t /*ts*/>>_acklist;
    BufferRaw::Ptr _buffer_pool;  //用于合并多个kcp包到一个udp包中
};
} // namespace toolkit

#endif // TOOLKIT_NETWORK_KCP_H
