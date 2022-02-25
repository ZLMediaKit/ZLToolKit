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
#include "Http/Body/AnyBody.hpp"
#include "HttpRequest.hpp"
#include "Network/Buffer.h"
#include <regex>
namespace http{
    class HttpSplitter{
    public:
        HttpSplitter():data_bytes(0){}

        virtual ~HttpSplitter(){}

        void clear();
        /*
         * return: 如果分包完毕，则返回整的一个包
         * */
        virtual void Input(const void* data, size_t len);
        virtual void OnRecvHeaderBody(std::string& header, std::string& body) = 0;
    private:
        size_t set_body_size();
        size_t getContentLength();
    private:
        //缓存的头
        std::string header;
        //缓存的数据区
        std::string body;
        //是否正在处于接收数据中，需要接收数据的字节数
        size_t data_bytes;
    };
}


#endif//FOLLOWVARY_HTTPSPLITTER_HPP
