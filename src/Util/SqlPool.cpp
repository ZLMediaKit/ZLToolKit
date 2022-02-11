/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_MYSQL)

#include <memory>
#include "util.h"
#include "onceToken.h"
#include "SqlPool.h"

using namespace std;

namespace toolkit {

INSTANCE_IMP(SqlPool)

} /* namespace toolkit */

#endif// defined(ENABLE_MYSQL)

