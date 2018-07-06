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
    public:
        BufferRaw::Ptr stamp_ptr;
        BufferRaw::Ptr data_ptr;
        const unsigned int STAMP_LENGTH = sizeof(unsigned int);
        unsigned int packet_len = 0;

        LengthTcpSession(const std::shared_ptr<ThreadPool> &th, const Socket::Ptr &sock) :
        TcpSession(th, sock) {
            stamp_ptr = obtainBuffer();
            data_ptr = obtainBuffer();
        }

        virtual void onRecv(const Buffer::Ptr &buf) override {
            char* buffer = buf->data();
            int buffer_len = strlen(buffer);
            if (packet_len == 0) {
                int stamp_size = _Chars2Int(buffer);
                data_ptr->setCapacity(stamp_size);
                while (buffer_len >= 0) {

                }
            } else if (packet_len > 0) {


            } else {
                throw invalid_argument("packet length is invalid, because it less than 0");
            }

            buffer = nullptr;
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
