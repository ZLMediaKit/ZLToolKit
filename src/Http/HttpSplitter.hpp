/*
* @file_name: HttpSplitter.hpp
* @date: 2021/12/06
* @author: oaho
* Copyright @ hz oaho, All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#ifndef FOLLOWVARY_HTTPSPLITTER_HPP
#define FOLLOWVARY_HTTPSPLITTER_HPP
#include <string>

namespace Http{
    class HttpSplitter{
    public:
        virtual ~HttpSplitter() = default;
        void input(const void* data, size_t length);
    public:
        /*
        * @description: 当收到整个http头时
        * @date: 2022/2/26
        * @param: 数据指针
        * @param: 头部的整个长度
        */
        virtual void onRecvHeader(const char* data, size_t length) = 0;
        /*
        * @description: 收到整个消息体回调
        * @date: 2022/2/26
        * @param: 数据指针
        * @return: 数据长度
        */
        virtual void onRecvBody(const char* body, size_t length) = 0;
    };


}


#endif//FOLLOWVARY_HTTPSPLITTER_HPP
