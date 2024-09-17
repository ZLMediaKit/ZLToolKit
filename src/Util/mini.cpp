/*
 * Copyright (c) 2015 r-lyeh (https://github.com/r-lyeh)
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 * 
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "mini.h"

using namespace std;

namespace toolkit {

template<>
mINI_basic<string, variant> &mINI_basic<string, variant>::Instance(){
    static mINI_basic<string, variant> instance;
    return instance;
}

template <>
bool variant::as<bool>() const {
    if (empty() || isdigit(front())) {
        //数字开头  [AUTO-TRANSLATED:e4266329]
        //Starts with a number
        return as_default<bool>();
    }
    if (strToLower(std::string(*this)) == "true") {
        return true;
    }
    if (strToLower(std::string(*this)) == "false") {
        return false;
    }
    //未识别字符串  [AUTO-TRANSLATED:b8037f51]
    //Unrecognized string
    return as_default<bool>();
}

template<>
uint8_t variant::as<uint8_t>() const {
    return 0xFF & as_default<int>();
}

}  // namespace toolkit


