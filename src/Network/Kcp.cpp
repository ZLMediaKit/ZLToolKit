/*
 * Copyright (c) 2021 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Kcp.h"
#include "Util/Byte.hpp"

using namespace std;

namespace toolkit {

static inline uint32_t _imin_(uint32_t a, uint32_t b) {
	return a <= b ? a : b;
}

static inline uint32_t _imax_(uint32_t a, uint32_t b) {
	return a >= b ? a : b;
}

static inline uint32_t _ibound_(uint32_t lower, uint32_t middle, uint32_t upper) {
	return _imin_(_imax_(lower, middle), upper);
}

static inline long _itimediff(uint32_t later, uint32_t earlier) {
	return ((int32_t)(later - earlier));
}

uint32_t getCurrent() {
    return (uint32_t)(getCurrentMillisecond() & 0xfffffffful);
}

////////////  KcpHeader //////////////////////////

bool KcpHeader::loadHeaderFromData(const char *data, size_t len) {
    if (HEADER_SIZE > len) {
        ErrorL << "size too smalle " << len;
        return false;
    }

    int offset = 0;
    _conv = Byte::Get4Bytes((const uint8_t*)data, 0);
    offset += 4;
    _cmd = (Cmd)Byte::Get1Byte((const uint8_t*)data, offset);
    offset += 1;
    _frg = Byte::Get1Byte((const uint8_t*)data, offset);
    offset += 1;
    _wnd = Byte::Get2Bytes((const uint8_t*)data, offset);
    offset += 2;
    _ts = Byte::Get4Bytes((const uint8_t*)data, offset);
    offset += 4;
    _sn = Byte::Get4Bytes((const uint8_t*)data, offset);
    offset += 4;
    _una = Byte::Get4Bytes((const uint8_t*)data, offset);
    offset += 4;
    _len = Byte::Get4Bytes((const uint8_t*)data, offset);

    return true;
}

////////////  KcpPacket //////////////////////////

KcpPacket::~KcpPacket() {
}

KcpPacket::Ptr KcpPacket::parse(const char* data, size_t len) {
    auto packet = std::make_shared<KcpPacket>();
    packet->loadFromData(data, len);
    return packet;
}

bool KcpPacket::loadFromData(const char *data, size_t len) {

    if (!loadHeaderFromData(data, len)) {
        return false;
    }

    auto packetSize = getPacketSize();
    if (len <= packetSize) {
        WarnL << "data len " << len << " is smaller than packet len :" << packetSize;
        return false;
    }

    assign((const char *)(data), packetSize);
    return true;
}

////////////  KcpTransport //////////////////////////
KcpTransport::KcpTransport(bool server_mode) {
    _server_mode = server_mode;
    if (!server_mode) {
        //客户端 conv 随机生成
        _conv = makeRandNum();
        _conv_init = true;
    }
    _buffer_pool = BufferRaw::create(_mtu);
}

KcpTransport::~KcpTransport() {
    update();
}

ssize_t KcpTransport::send(const Buffer::Ptr &buf, bool flush) {

    if (!_poller) {
        startTimer();
    }

    if (!_conv_init) {
        WarnL << "conv should set before send";
        return -1;
    }

    auto data = buf->data();
    auto size = buf->size();
    if (size <= 0) {
        return 0;
    }

    if (size >= _mss * IKCP_WND_RCV) {
        //分片过大,拒绝发送
        return -1;
    }

    auto leftLen = size;
    auto extendLen = mergeSendQueue(data, leftLen);
    data += extendLen;
    leftLen -= extendLen;

    // fragment
    int count = (leftLen + _mss - 1) / _mss;
    for (int i = 0; i < count; i++) {
        auto len = std::min<size_t>(leftLen, _mss);
        auto packet = std::make_shared<KcpDataPacket>(_conv, len);
        memcpy(packet->getPayloadData(), data, len);
        packet->setFrg(!_stream? (count - i - 1) : 0);
        _snd_queue.push_back(packet);

        data += len;
        leftLen -= len;
    }

    if (flush) {
        sendSendQueue();
    }
    return size;
}

void KcpTransport::input(const Buffer::Ptr &buf) {

    if (!_poller) {
        startTimer();
    }

    auto data = buf->data();
    auto size = buf->size();
    uint32_t current = getCurrent();
	uint32_t prev_una = _snd_una;
	uint32_t maxack = 0;
	uint32_t latest_ts = 0;
	bool fastAckFlag = false;
	bool hasData = false;

	while (1) {
        auto packet = KcpPacket::parse(data, size);
        if (!packet) {
            WarnL << "parse kcp packet fail";
            break;
        }
		data += packet->size();
		size -= packet->size();

        if (!_conv_init) {
            _conv = packet->getConv();
            _conv_init = true;
        } else {
            if (_conv != packet->getConv()) {
                WarnL << "_conv check fail, skip this packet";
                continue;
            }
        }

        auto cmd = packet->getCmd();
		if (cmd != KcpHeader::Cmd::CMD_PUSH && cmd != KcpHeader::Cmd::CMD_ACK &&
			cmd != KcpHeader::Cmd::CMD_WASK && cmd != KcpHeader::Cmd::CMD_WINS) {
            WarnL << "unknow cmd: " << (uint8_t)cmd;
            continue;
        }

        handleAnyPacket(packet);

        switch (cmd) {
            case KcpHeader::Cmd::CMD_ACK: {
                auto sn = packet->getSn();
                auto ts = packet->getTs();
                handleCmdAck(packet, current);
                if (!fastAckFlag) {
                    fastAckFlag = true;
                    maxack = sn;
                    latest_ts = ts;
                } else {
                    if (sn > maxack) {
                        if (!_fastack_conserve || ts > latest_ts) {
                            //激进模式
                            maxack = sn;
                            latest_ts = ts;
                        }
                    }
                }
            }
                break;
            case KcpHeader::Cmd::CMD_PUSH:
                handleCmdPush(packet);
                hasData = true;
                break;
            case KcpHeader::Cmd::CMD_WASK:
                _probe |= IKCP_ASK_TELL;
                break;
            case KcpHeader::Cmd::CMD_WINS:
            default:
                WarnL << "unknow cmd: " << (uint8_t)cmd;
                break;
        }
	}

	if (fastAckFlag) {
		updateFastAck(maxack, latest_ts);
	}

    if (_snd_una > prev_una) {
        //有新的应答,尝试增大拥塞窗口
        increaseCwnd();
    }

    if (hasData) {
        onData();
    }

	return;
}

void KcpTransport::startTimer() {
    if (!_poller) {
        _poller = EventPoller::getCurrentPoller();
    }

    std::weak_ptr<KcpTransport> weak_self = std::static_pointer_cast<KcpTransport>(shared_from_this());
    float interval = float(_interval)/ 1000.0;
    _timer = std::make_shared<Timer>(interval, [weak_self]() -> bool {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return false;
        }
        strong_self->update();
        return true;
    }, _poller);
    return;
}

void KcpTransport::onData() {
    bool fastRecover = false;

    sortRecvBuf();

    if (_rcv_queue.size() >= _rcv_wnd) {
        //接受队列当前超过接收窗口大小
        fastRecover = true;
    }

    // merge fragment
    while (int size = peeksize()) {
        while (1) {
            int offset = 0;
            auto buffer = BufferRaw::create(size);
            auto packet = _rcv_queue.front();
            _rcv_queue.pop_front();
            memcpy(buffer->data() + offset, packet->getPayloadData(), packet->getLen());
            offset += packet->getLen();

            if (packet->getFrg() == 0) {
                onRead(buffer);
                break;
            }
        }
    }

    // fast recover
    if (_rcv_queue.size() < _rcv_wnd && fastRecover) {
        // ready to send back IKCP_CMD_WINS
        // tell remote my window size
        _probe |= IKCP_ASK_TELL;
    }
	return;
}

int KcpTransport::peeksize() {
	if (_rcv_queue.empty()) {
        return 0;
    }

    //分包数据还没发送完全
	if (_rcv_queue.size() < _rcv_queue.front()->getFrg() + 1) {
        return 0;
    }

	int length = 0;
	for (auto it = _rcv_queue.begin(); it != _rcv_queue.end(); it++) {
        auto seg = *it;
		length += seg->getLen();
		if (seg->getFrg() == 0) {
            break;
        }
	}

	return length;
}

// move available data from rcv_buf -> rcv_queue
void KcpTransport::sortRecvBuf() {
#if 0
    //直送应用层,不考虑接受队列满的情况
    if (_rcv_queue.size() >= _rcv_wnd) {
        //接收队列满
        return;
    }
#endif

    while (!_rcv_buf.empty()) {
        auto packet = _rcv_buf.front();
        if (packet->getSn() == _rcv_nxt) {
            //接收缓存中序号正确,且接受队列窗口足够
            //将接收缓存中的包转到接受队列中
            _rcv_buf.pop_front();
            _rcv_queue.push_back(packet);
            _rcv_nxt++;
        } else {
            break;
        }
    }

    return;
}

// move data from snd_queue to snd_buf
void KcpTransport::sortSendQueue() {
    uint32_t current = getCurrent();

    uint32_t cwnd = _imin_(_snd_wnd, _rmt_wnd);
    if (_nocwnd == 0) {
        cwnd = _imin_(_cwnd, cwnd);
    }

    while (_snd_nxt < _snd_una + cwnd) {
        if (_snd_queue.empty()) {
            break;
        }

        auto packet = _snd_queue.front();
        _snd_queue.pop_front();

		packet->setConv(_conv);
		packet->setCmd(KcpHeader::Cmd::CMD_PUSH);
		packet->setWnd(getWaitSnd());
		packet->setTs(current);
		packet->setSn(_snd_nxt++);
		packet->setUna(_rcv_nxt);
		packet->setResendts(current);
		packet->setRto(_rx_rto);
		packet->setFastack(0);
		packet->setXmit(0);

        _snd_buf.push_back(packet);
	}
    return;
}

size_t KcpTransport::mergeSendQueue(const char *buffer, size_t len) {
	if (len <= 0) {
        return 0;
    }

    // 流发送模式,表示可以将当前buffer合并之前的包后面
    if (!_stream) {
        return 0;
    }

    //发送队列没有数据,不用合并
    if (_snd_queue.empty()) {
        return 0;
    }

    auto packet = _snd_queue.front();
    size_t oldLen = packet->getLen();
    if (oldLen >= _mss) {
        //前一个包已经达到_mss长度,不允许合并
        return 0;
    }

    size_t extendLen = std::min<size_t>(len, _mss - oldLen);
    packet->setPayLoadSize(oldLen + extendLen);
    memcpy(packet->getPayloadData() + oldLen, buffer, extendLen);
    packet->setLen(oldLen + extendLen);
    packet->setFrg(0);
    return extendLen;
}

void KcpTransport::updateRtt(int32_t rtt) {
    if (rtt < 0) {
        return;
    }

    int32_t rto = 0;
    //Jacobson/Karels RTT估算算法
    if (_rx_srtt == 0) {
        _rx_srtt = rtt;
        _rx_rttval = rtt / 2;
    } else {
        long delta = abs(rtt - _rx_srtt);
        _rx_rttval = (3 * _rx_rttval + delta) / 4;
        _rx_srtt = (7 * _rx_srtt + rtt) / 8;
        if (_rx_srtt < 1) {
            _rx_srtt = 1;
        }
    }

    rto = _rx_srtt + _imax_(_interval, 4 * _rx_rttval);
    _rx_rto = _ibound_(_rx_minrto, rto, IKCP_RTO_MAX);

    return;
}

void KcpTransport::dropCacheByUna(uint32_t una) {
    while (!_snd_buf.empty()) {
        if (una <= _snd_buf.front()->getSn()) {
            break;
        }
        _snd_buf.pop_front();
    }

    _snd_una = _snd_buf.empty()? _snd_nxt : _snd_buf.front()->getSn();
    return;
}

void KcpTransport::dropCacheByAck(uint32_t sn) {
	for (auto it = _snd_buf.begin(); it != _snd_buf.end(); it++) {
		if (sn < (*it)->getSn()) {
			break;
		} else if (sn == (*it)->getSn()) {
            _snd_buf.erase(it);
			break;
        }
	}

    _snd_una = _snd_buf.empty()? _snd_nxt : _snd_buf.front()->getSn();
    return;
}

void KcpTransport::updateFastAck(uint32_t sn, uint32_t ts) {
    if (sn < _snd_una || sn >= _snd_nxt) {
        return;
    }

    for (auto it = _snd_buf.begin(); it != _snd_buf.end(); it++) {
        auto seg = *it;
        if (sn < seg->getSn()) {
            break;
        } else if (sn != seg->getSn()) {
            if (!_fastack_conserve || ts > seg->getTs()) {
                seg->setFastack(seg->getFastack() + 1);
            }
        }
    }
    return;
}

void KcpTransport::increaseCwnd() {
    if (_cwnd >= _rmt_wnd) {
        return;
    }

    uint32_t mss = _mss;
    if (_cwnd < _ssthresh) {
        //慢启动阶段,拥塞窗口指数增长
        _cwnd++;
        _incr += mss;
    } else {
        //拥塞避免阶段,拥塞窗口线性增长
        if (_incr < mss) {
            _incr = mss;
        }

        _incr += (mss * mss) / _incr + (mss / 16);
        if ((_cwnd + 1) * mss <= _incr) {
#if 1
            _cwnd = (_incr + mss - 1) / ((mss > 0)? mss : 1);
#else
            _cwnd++;
#endif
        }
    }

    //控制不超过远端窗口大小
    if (_cwnd > _rmt_wnd) {
        _cwnd = _rmt_wnd;
        _incr = _rmt_wnd * mss;
    }
    return;
}

void KcpTransport::handleAnyPacket(KcpPacket::Ptr packet) {
    _rmt_wnd = packet->getWnd();
    dropCacheByUna(packet->getUna());
    return;
}

void KcpTransport::handleCmdAck(KcpPacket::Ptr packet, uint32_t current) {
    updateRtt(current - packet->getTs());
    dropCacheByAck(packet->getSn());
    return;
}

void KcpTransport::handleCmdPush(KcpPacket::Ptr packet) {
    auto sn = packet->getSn();
    auto ts = packet->getTs();

    if (sn >= _rcv_nxt + _rcv_wnd) {
        WarnL << "sn: " << sn << "is over wnd, _rcv_nxt: " << _rcv_nxt << ":, skip";
        //超出接受窗口数据
        return;
    }

    _acklist.push_back(std::make_pair(sn, ts));
    if (sn <= _rcv_nxt) {
        WarnL << "sn: " << sn << "is small than _rcv_nxt: " << _rcv_nxt << ":, skip";
        return;
    }

	bool repeat = false;
    auto it = _rcv_buf.begin();
    for (; it != _rcv_buf.end(); it++) {
        auto packet = *it;
		if (packet->getSn() == sn) {
            DebugL << "sn: " << sn << "is repeat skip";
			repeat = true;
			break;
		}

		if (sn > packet->getSn()) {
			break;
		}
	}

	if (!repeat) {
        _rcv_buf.insert(it, packet);
	}

    return;
}

//获取当前空闲接受队列窗口
int KcpTransport::getRcvWndUnused() {
    auto wnd = _rcv_wnd - _rcv_queue.size();
    if (wnd > 0) {
        return wnd;
    }
	return 0;
}


void KcpTransport::update() {
    sendAckList();
    sendProbePacket();
    sendSendQueue();
}

void KcpTransport::sendSendQueue() {
    uint32_t resent;
    uint32_t rtomin;
    bool change = false;
    bool lost = false;
    uint32_t current = getCurrent();

    sortSendQueue();

    // calculate resent
    resent = (_fastresend > 0)? (uint32_t)_fastresend : 0xffffffff;
    rtomin = (_delay_mode == DelayMode::DELAY_MODE_NORMAL)? (_rx_rto >> 3) : 0;

    // flush data segments
    for (auto it = _snd_buf.begin(); it != _snd_buf.end(); it++) {
        bool needsend = false;

        auto packet = *it;
        auto xmit = packet->getXmit();
        //没重传过,第一次发送数据包
        if (xmit == 0) {
            needsend = true;
            packet->setXmit(xmit + 1);
            packet->setRto(_rx_rto);
            packet->setResendts(current + _rx_rto + rtomin);
        } else if (current >= packet->getResendts()) {
            //普通重传
            needsend = true;
            packet->setXmit(xmit + 1);
            _xmit++;
            auto rto = packet->getRto();
            if (_delay_mode == DelayMode::DELAY_MODE_NORMAL == 0) {
                packet->setRto(rto + _imax_(rto, (uint32_t)_rx_rto));
            } else {
                int32_t step = (_delay_mode == DelayMode::DELAY_MODE_FAST)? ((int32_t)(rto)) : _rx_rto;
                packet->setRto(rto + step / 2);
            }
            packet->setResendts(current + rto);
            lost = true;
        } else if (packet->getFastack() >= resent) {
            //快速重传
            if ((int)xmit <= _fastlimit || _fastlimit <= 0) {
                auto rto = packet->getRto();
                needsend = true;
                packet->setXmit(xmit + 1);
                packet->setFastack(0);
                packet->setResendts(current + rto);
                change = true;
            }
        }

        if (needsend) {
            int need;
            packet->setTs(current);
            packet->setWnd(getRcvWndUnused());
            packet->setUna(_rcv_nxt);

            if (packet->getXmit() >= _dead_link) {
                _state = STATE_TIMEOUT;
            }

            sendPacket(packet);
        }
    }

    flushPool();

    decreaseCwnd(change, lost);
    return;
}

void KcpTransport::sendAckList() {
    while (!_acklist.empty()) {
        auto front = _acklist.front();
        _acklist.pop_front();

        auto packet = std::make_shared<KcpAckPacket>(_conv);
        packet->setWnd(getRcvWndUnused());
        packet->setUna(_rcv_nxt);
        packet->setSn(front.first);
        packet->setTs(front.second);
        sendPacket(packet);
    }
    return;
}

void KcpTransport::sendProbePacket() {
	uint32_t current = getCurrent();
 
    // probe window size (if remote window size equals zero)
    if (_rmt_wnd == 0) {
        if (_probe_wait == 0) {
            _probe_wait = IKCP_PROBE_INIT;
            _ts_probe = current + _probe_wait;
        } else {
            if (_itimediff(current, _ts_probe) >= 0) {
                if (_probe_wait < IKCP_PROBE_INIT) {
                    _probe_wait = IKCP_PROBE_INIT;
                }
                _probe_wait += _probe_wait / 2;
                if (_probe_wait > IKCP_PROBE_LIMIT) {
                    _probe_wait = IKCP_PROBE_LIMIT;
                }
                _ts_probe = current + _probe_wait;
                _probe |= IKCP_ASK_SEND;
            }
        }
    } else {
        _ts_probe = 0;
        _probe_wait = 0;
    }

    // flush window probing commands
    if (_probe & IKCP_ASK_SEND) {
        auto packet = std::make_shared<KcpProbePacket>(_conv);
        sendPacket(packet);
	}

	// flush window probing commands
	if (_probe & IKCP_ASK_TELL) {
        auto packet = std::make_shared<KcpTellPacket>(_conv);
        sendPacket(packet);
	}

	_probe = 0;
    return;
}

int KcpTransport::getWaitSnd() {
	return _snd_buf.size() + _snd_queue.size();
}

// update ssthresh
void KcpTransport::decreaseCwnd(bool change, bool lost) {
    //处理因为快速重传或者丢包的情况下,进行拥塞窗口处理

	uint32_t resent = (_fastresend > 0)? (uint32_t)_fastresend : 0xffffffff;

    // calculate window size
    uint32_t cwnd = _imin_(_snd_wnd, _rmt_wnd);
    if (_nocwnd == 0) {
        cwnd = _imin_(_cwnd, cwnd);
    }

	//快速重传表明网络出现轻微拥塞，采用相对温和的调整策略。
    //主动降低发送速率,但不是因为实际的丢包(可能是乱序)
    if (change) {
		//调整慢启动阈值为在途数据量的一半
        uint32_t inflight = _snd_nxt - _snd_una;
        _ssthresh = inflight / 2;
        if (_ssthresh < IKCP_THRESH_MIN) {
            _ssthresh = IKCP_THRESH_MIN;
        }
        _cwnd = _ssthresh + resent;
        _incr = _cwnd * _mss;
    }

	//超时重传表明网络严重拥塞，采用激进的调整策略。
    if (lost) {
        _ssthresh = cwnd / 2;
        if (_ssthresh < IKCP_THRESH_MIN) {
            _ssthresh = IKCP_THRESH_MIN;
        }
		//重置拥塞窗口,回到慢启动阶段
        _cwnd = 1;
        _incr = _mss;
    }

    if (_cwnd < 1) {
        _cwnd = 1;
        _incr = _mss;
    }
    return;
}

void KcpTransport::setMtu(int mtu) {
	if (mtu < 50 || mtu < KcpHeader::HEADER_SIZE) {
        std::string err = (StrPrinter << "kcp setMtu " << mtu << "to small");
        throw std::runtime_error(err);
    }

	_mtu = mtu;
	_mss = _mtu - KcpHeader::HEADER_SIZE;
	return;
}

void KcpTransport::setInterval(int interval) {
    _interval = _ibound_(10, interval, 5000);
	return;
}

void KcpTransport::setDelayMode(DelayMode delay_mode) {
    if (delay_mode < DelayMode::DELAY_MODE_NORMAL 
        || delay_mode > DelayMode::DELAY_MODE_NO_DELAY) {
        return;
    }

    _delay_mode = delay_mode;
    if (delay_mode == DelayMode::DELAY_MODE_NORMAL) {
        _rx_minrto = IKCP_RTO_MIN;
    } else {
        _rx_minrto = IKCP_RTO_NDL;
    }
	return;
}

void KcpTransport::setFastackConserve(bool flag) {
    _fastack_conserve = flag;
    return;
}

void KcpTransport::setNoCwnd(bool flag) {
    _nocwnd = flag;
    return;
}

void KcpTransport::setStreamMode(bool flag) {
    _stream = flag;
    return;
}

void KcpTransport::setFastResend(int resend) {
    _fastresend = resend;
    return;
}

void KcpTransport::setWndSize(int sndwnd, int rcvwnd) {
    if (sndwnd > 0) {
        _snd_wnd = sndwnd;
    }
    if (rcvwnd > 0) {   // must >= max fragment size
        _rcv_wnd = _imax_(rcvwnd, IKCP_WND_RCV);
    }
	return;
}

void KcpTransport::sendPacket(Buffer::Ptr pkt, bool flush) {
    if (pkt->size() + _buffer_pool->size() > _mtu) {
        flushPool();
    }

    memcpy(_buffer_pool->data() + _buffer_pool->size(), pkt->data(), pkt->size());
    _buffer_pool->setSize(_buffer_pool->size() + pkt->size());

    if (flush) {
        flushPool();
    }
    return;
}

void KcpTransport::flushPool() {
    onWrite(_buffer_pool);
    _buffer_pool->setSize(0);
}

} // namespace toolkit
