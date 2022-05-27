/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdlib>
#include "Buffer.h"
#include "Util/onceToken.h"

namespace toolkit {

StatisticImp(Buffer)
StatisticImp(BufferRaw)
StatisticImp(BufferLikeString)

BufferRaw::Ptr BufferRaw::create() {
#if 0
    static ResourcePool<BufferRaw> packet_pool;
    static onceToken token([]() {
        packet_pool.setSize(1024);
    });
    auto ret = packet_pool.obtain2();
    ret->setSize(0);
    return ret;
#else
    return Ptr(new BufferRaw);
#endif
}

}//namespace toolkit
