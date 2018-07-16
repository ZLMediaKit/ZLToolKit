//
// Created by rbcheng on 18-7-5.
// Email: rbcheng@qq.com
//

#ifndef ZLTOOLKIT_LENGTHTCPSESSION_H
#define ZLTOOLKIT_LENGTHTCPSESSION_H

#include "TcpSession.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Thread;

namespace ZL {
namespace Network {

    class LengthTcpSession : public TcpSession {
    private:
        BufferRaw::Ptr stamp_ptr;
        BufferRaw::Ptr data_ptr;
        const unsigned int STAMP_LENGTH = sizeof(unsigned int);
        const short ZERO = 0;
        const short ONE = 1;

        void reset_stamp_and_data() noexcept {
            stamp_ptr->setCapacity(STAMP_LENGTH);
            stamp_ptr->setSize(ZERO);
            data_ptr->setCapacity(ZERO);
            data_ptr->setSize(ZERO);
        }

        void handle_buffer(char* data, int& size) {
            char* buffer = data;
            if (size <=0) {
                buffer = nullptr;
                return;
            }

            if (size >= STAMP_LENGTH) {
                if (stamp_ptr->size() < stamp_ptr->capacity()) {
                    int need_len = stamp_ptr->capacity() - stamp_ptr->size();
                    stamp_ptr->append(buffer, need_len);
                    buffer += need_len;
                    size -= need_len;
                }

                int stamp = _Chars2Int(stamp_ptr->data());
                InfoL << stamp;
                if (stamp < 0) {
                    buffer = nullptr;
                    reset_stamp_and_data();
                    std::string stamp_error = "stamp less than 0: " + to_string(stamp);
                    throw invalid_argument(stamp_error);
                } else if (stamp == 0) {
                    buffer = nullptr;
                    reset_stamp_and_data();
                    return;
                }

                int buffer_len = size;
                if (buffer_len == 0) {
                    buffer = nullptr;
                    return;
                }
                if (data_ptr->capacity() == 0) {
                    // 数据缓冲区未被初始化
                    data_ptr->setCapacity(stamp);
                    if (buffer_len == stamp) {
                        // 缓冲区长度刚好等于标志位长度
                        data_ptr->append(buffer, stamp);
                        onRecvOnePacket(data_ptr);
                        reset_stamp_and_data();
                        buffer = nullptr;
                        return;
                    } else if (buffer_len < stamp) {
                        // 缓冲区内容不足，长度小于标志位长度
                        data_ptr->append(buffer, buffer_len);
                        buffer = nullptr;
                        return;
                    } else if (buffer_len > stamp) {
                        // 缓冲区内容太多，长度大于标志位长度
                        data_ptr->append(buffer, stamp);
                        buffer += stamp;
                        size -= stamp;
                        onRecvOnePacket(data_ptr);
                        reset_stamp_and_data();
                        handle_buffer(buffer, size);
                    }
                } else if (data_ptr->capacity() == stamp) {
                    // 数据缓冲区已经初始化
                    int need_len = data_ptr->capacity() - data_ptr->size();
                    if (buffer_len == need_len) {
                        data_ptr->append(buffer, need_len);
                        onRecvOnePacket(data_ptr);
                        reset_stamp_and_data();
                        buffer = nullptr;
                        return;
                    } else if (buffer_len < need_len) {
                        data_ptr->append(buffer, buffer_len);
                        buffer = nullptr;
                        return;
                    } else if (buffer_len > need_len) {
                        data_ptr->append(buffer, need_len);
                        buffer += need_len;
                        size -= need_len;
                        onRecvOnePacket(data_ptr);
                        reset_stamp_and_data();
                        handle_buffer(buffer, size);
                    }
                } else {
                    buffer = nullptr;
                    throw invalid_argument("data capacity is invalid.");
                }
            } else {
                // 每次处理一个字节的方式来处理小于{@param STAMP_LENGTH}的字节流
                if (stamp_ptr->size() < stamp_ptr->capacity()) {
                    // 当标识符缓冲去位装满，对其一直装载，直到将其装满，再继续对数据流进行处理
                    stamp_ptr->append(buffer, ONE);
                    buffer += ONE;
                    size -= ONE;
                    if (stamp_ptr->size() == stamp_ptr->capacity()) {
                        buffer = nullptr;
                        return;
                    } else {
                        handle_buffer(buffer, size);
                    }

                }

                if (stamp_ptr->size() == stamp_ptr->capacity()) {
                    // 判断buffer是否还有需要处理的字节流
                    if (size == 0) {
                        buffer = nullptr;
                        return;
                    }

                    int stamp = _Chars2Int(stamp_ptr->data());
                    if (stamp < 0) {
                        buffer = nullptr;
                        std::string stamp_error = "stamp less than 0: " + to_string(stamp);
                        throw invalid_argument(stamp_error);
                    } else if (stamp == 0) {
                        buffer = nullptr;
                        return;
                    }

                    if (data_ptr->capacity() == 0) {
                        data_ptr->setCapacity(stamp);
                        data_ptr->append(buffer, ONE);
                        buffer += ONE;
                        size -= ONE;
                        if (data_ptr->size() == data_ptr->capacity()) {
                            onRecvOnePacket(data_ptr);
                            reset_stamp_and_data();
                        } else {
                            handle_buffer(buffer, size);
                        }

                    } else if (data_ptr->capacity() == stamp) {
                        data_ptr->append(buffer, ONE);
                        buffer += ONE;
                        size -= ONE;
                        if (data_ptr->size() == data_ptr->capacity()) {
                            onRecvOnePacket(data_ptr);
                            reset_stamp_and_data();
                        }

                        handle_buffer(buffer, size);
                    } else {
                        buffer = nullptr;
                        reset_stamp_and_data();
                        throw invalid_argument("data capacity is invalid.");
                    }


                }
            }

        }
    public:

        typedef std::shared_ptr<LengthTcpSession> Ptr;

        LengthTcpSession(const std::shared_ptr<ThreadPool> &th, const Socket::Ptr &sock) :
        TcpSession(th, sock) {
            stamp_ptr = obtainBuffer();
            data_ptr = obtainBuffer();
            reset_stamp_and_data();
        }

        virtual void onRecv(const Buffer::Ptr &buf) override {
            DebugL << buf->data();
            DebugL << buf->size();
            int buf_size = buf->size();
            handle_buffer(buf->data(), buf_size);
        }

        virtual void onError(const SockException &err) override {
            reset_stamp_and_data();
            WarnL << err.what();
        }

        virtual void onManager() override {

        }

        virtual void onRecvOnePacket(const BufferRaw::Ptr& buf) {
            DebugL << buf->data();
            (*this) << buf->data();
        }

        ~LengthTcpSession();

    };
}
}

#endif //ZLTOOLKIT_LENGTHTCPSESSION_H
