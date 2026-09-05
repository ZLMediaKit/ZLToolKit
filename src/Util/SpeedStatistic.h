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

#include <cstdint>
#include "TimeTicker.h"

namespace toolkit {

class BytesSpeed {
public:
    BytesSpeed(uint64_t window_ms = 5000, uint64_t min_sample_ms = 100, uint64_t init_sample_ms = 1000)
        : _window_ms(window_ms), _min_sample_ms(min_sample_ms), _init_sample_ms(init_sample_ms) {}
    ~BytesSpeed() = default;

    /**
     * 添加统计字节
     * Add statistical bytes
     
     * [AUTO-TRANSLATED:d6697ac9]
     */
    BytesSpeed &operator+=(size_t bytes) {
        _bytes += bytes;
        _total_bytes += bytes;
        return *this;
    }

    /**
     * 获取速度，单位bytes/s
     * Get speed, unit bytes/s
     
     * [AUTO-TRANSLATED:41e26e29]
     */
    size_t getSpeed() {
        auto elapsed = _ticker.elapsedTime();
        if (elapsed < _min_sample_ms) {
            // 获取频率过高，那么返回上次计算结果
            // Query too frequently, return the last calculation result
            return _speed;
        }
        auto bytes = _bytes;
        if (!_has_speed) {
            if (elapsed < _init_sample_ms) {
                return 0;
            }
            _speed = (size_t)((uint64_t)bytes * 1000 / elapsed);
        } else if (elapsed >= _window_ms) {
            _speed = (size_t)((uint64_t)bytes * 1000 / elapsed);
        } else {
            // EMA递推: speed += (bytes*1000 - speed*elapsed) / window_ms
            auto old = (int64_t)_speed;
            auto diff = (int64_t)((uint64_t)bytes * 1000) - old * (int64_t)elapsed;
            auto res = old + diff / (int64_t)_window_ms;
            _speed = (size_t)(res < 0 ? 0 : res);
        }
        _bytes = 0;
        _ticker.resetTime();
        _has_speed = true;
        return _speed;
    }

    size_t getTotalBytes() const {
        return _total_bytes;
    }

private:
    uint64_t _window_ms = 5000;
    uint64_t _min_sample_ms = 100;
    uint64_t _init_sample_ms = 1000;
    size_t _speed = 0;
    size_t _bytes = 0;
    size_t _total_bytes = 0;
    bool _has_speed = false;
    Ticker _ticker;
};

} /* namespace toolkit */
#endif /* SPEED_STATISTIC_H_ */
