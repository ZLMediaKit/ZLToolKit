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
#include <functional>
namespace Http{

    class HttpSplitter{
    public:
        HttpSplitter();
        virtual ~HttpSplitter() = default;
        void input(const void* data, size_t length);
    public:
        /*
        * @description: 当收到整个http头时
        * @date: 2022/2/26
        * @param: 数据
        */
        virtual void onRecvHeader(std::string& header) = 0;
        /*
        * @description: 收到整个消息体回调
        * @date: 2022/2/26
        * @param: 数据指针
        * @return: 数据长度
        */
        virtual void onRecvBody(std::string& body) = 0;

        /*
        * @description: 收到chunked消息
        * @date: 2022/2/26
        * @param: body 消息体头指针
        * @param: length 长度
        */
        virtual void onRecvChunkedBody(std::string& body) = 0;
        /*
         * 当收到chunked结束的消息
         * **/
        virtual void onRecvChunkedBodyTailer() = 0;
      private:
        /*
         * 收到头字段
         * */
        void onHeader(const char* data, size_t length);
        /*
         * 收到了body
         * */
        void onBody(const char* data, size_t length);
        /*
         * 收到了chunked body
         * */
        void onChunkedBody(const char* data, size_t length);
        /*
         *  在chunked 长度状态
         * */
        void onChunkedBody_l(const char* data, size_t length);
        void onChunkedBody_l2(const char* data, size_t length);
        /*
         * chunked末尾
         * */
        void onChunkedTailer(const char* data, size_t length);
        /*
         * 清空上下文
         * */
        void clear();
      private:
        /* 缓冲头部字段 */
        std::string header;
        /* 缓冲内容 */
        std::string body;
        /* 还需读入的字节 */
        size_t need_length = 0;
        /* 当前处于何种解析状态 */
        std::function<void(const char*, size_t)> _next_func;
    };


}


#endif//FOLLOWVARY_HTTPSPLITTER_HPP
