/*
* Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
*
* This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
*
* Use of this source code is governed by MIT license that can be found in the
* LICENSE file in the root of the source tree. All contributing project authors
* may be found in the AUTHORS file in the root of the source tree.
*/

#ifndef SPEED_STATISTIC_H_
#define SPEED_STATISTIC_H_

#include "TimeTicker.h"

namespace toolkit {

class BytesSpeed {
public:
    BytesSpeed() = default;
    ~BytesSpeed() = default;

    /**
     * 添加统计字节
     * Add statistical bytes
     
     * [AUTO-TRANSLATED:d6697ac9]
     */
    BytesSpeed &operator+=(size_t bytes) {
        _bytes += bytes;
        if (_bytes > 1024 * 1024) {
            // 数据大于1MB就计算一次网速  [AUTO-TRANSLATED:897af4d6]
            // Data greater than 1MB is calculated once for network speed
            computeSpeed();
        }
        _total_bytes +=  bytes;
        return *this;
    }

    /**
     * 获取速度，单位bytes/s
     * Get speed, unit bytes/s
     
     * [AUTO-TRANSLATED:41e26e29]
     */
    size_t getSpeed() {
        if (_ticker.elapsedTime() < 1000) {
            // 获取频率小于1秒，那么返回上次计算结果  [AUTO-TRANSLATED:b687b762]
            // Get frequency less than 1 second, return the last calculation result
            return _speed;
        }
        return computeSpeed();
    }

    size_t getTotalBytes() const {
        return _total_bytes;
    }

private:
    size_t computeSpeed() {
        auto elapsed = _ticker.elapsedTime();
        if (!elapsed) {
            return _speed;
        }
        _speed = (size_t)(_bytes * 1000 / elapsed);
        _ticker.resetTime();
        _bytes = 0;
        return _speed;
    }

private:
    size_t _speed = 0;
    size_t _bytes = 0;
    size_t _total_bytes = 0;
    Ticker _ticker;
};

} /* namespace toolkit */
#endif /* SPEED_STATISTIC_H_ */
