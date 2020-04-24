/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef UTIL_ONCETOKEN_H_
#define UTIL_ONCETOKEN_H_

#include <functional>
#include <type_traits>
using namespace std;

namespace toolkit {

class onceToken {
public:
    typedef function<void(void)> task;
    onceToken(const task &onConstructed, const task &onDestructed = nullptr) {
        if (onConstructed) {
            onConstructed();
        }
        _onDestructed = onDestructed;
    }
    onceToken(const task &onConstructed, task &&onDestructed) {
        if (onConstructed) {
            onConstructed();
        }
        _onDestructed = std::move(onDestructed);
    }
    ~onceToken() {
        if (_onDestructed) {
            _onDestructed();
        }
    }
private:
    onceToken();
    onceToken(const onceToken &);
    onceToken(onceToken &&);
    onceToken &operator =(const onceToken &);
    onceToken &operator =(onceToken &&);
    task _onDestructed;
};

} /* namespace toolkit */
#endif /* UTIL_ONCETOKEN_H_ */
