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
        void handle_buffer(char* buffer, int size) {
            if (size >= STAMP_LENGTH) {
                if (stamp_ptr->size() < stamp_ptr->capacity()) {
                    stamp_ptr->append(buffer, stamp_ptr->capacity() - stamp_ptr->size());
                    buffer +=(stamp_ptr->capacity() - stamp_ptr->size());
                }
                int stamp = _Chars2Int(stamp_ptr->data());
                int buffer_len = strlen(buffer);
                if (data_ptr->capacity() == 0) {
                    data_ptr->setCapacity(stamp);
                    if (buffer_len == stamp) {
                        data_ptr->append(buffer, stamp);
                        onRecvOnePacket(data_ptr);
                        stamp_ptr->setCapacity(STAMP_LENGTH);
                        return;
                    } else if (buffer_len < stamp) {
                        data_ptr->append(buffer, buffer_len);
                        return;
                    } else if (buffer_len > stamp) {
                        data_ptr->append(buffer, stamp);
                        buffer += stamp;
                        onRecvOnePacket(data_ptr);
                        stamp_ptr->setCapacity(STAMP_LENGTH);
                        handle_buffer(buffer, strlen(buffer));
                    }
                } else if (data_ptr->capacity() == stamp) {
                    int need_len = data_ptr->capacity() - data_ptr->size();
                    if (buffer_len == need_len) {
                        data_ptr->append(buffer, need_len);
                        onRecvOnePacket(data_ptr);
                        stamp_ptr->setCapacity(STAMP_LENGTH);
                        return;
                    } else if (buffer_len < need_len) {
                        data_ptr->append(buffer, buffer_len);
                        return;
                    } else if (buffer_len > need_len) {
                        data_ptr->append(buffer, need_len);
                        buffer += need_len;
                        onRecvOnePacket(data_ptr);
                        stamp_ptr->setCapacity(STAMP_LENGTH);
                        handle_buffer(buffer, strlen(buffer));
                    }
                } else {
                    throw invalid_argument("data capacity is invalid.");
                }
            } else {
                // TODO
            }

        }
    public:
        BufferRaw::Ptr stamp_ptr;
        BufferRaw::Ptr data_ptr;
        const unsigned int STAMP_LENGTH = sizeof(unsigned int);

        LengthTcpSession(const std::shared_ptr<ThreadPool> &th, const Socket::Ptr &sock) :
        TcpSession(th, sock) {
            stamp_ptr = obtainBuffer();
            stamp_ptr->setCapacity(STAMP_LENGTH);
            data_ptr = obtainBuffer();
        }

        virtual void onRecv(const Buffer::Ptr &buf) override {
            handle_buffer(buf->data(), buf->size());
        }

        virtual void onError(const SockException &err) override {
            WarnL << err.what();
        }

        virtual void onManager() override {

        }

        virtual void onRecvOnePacket(const BufferRaw::Ptr& buf) {

        }

        ~LengthTcpSession();

    };
}
}

#endif //ZLTOOLKIT_LENGTHTCPSESSION_H
