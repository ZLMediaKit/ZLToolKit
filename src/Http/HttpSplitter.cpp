/*
* @file_name: HttpSplitter.cpp
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
#include "HttpSplitter.hpp"
#include "Util/util.h"
#include <string.h>
#include <vector>
#include <set>
#include <regex>
#include <Util/logger.h>
using namespace toolkit;
namespace http{

    void HttpSplitter::clear() {
        header.clear();
        body.clear();
        data_bytes = 0;
    }

    void HttpSplitter::Input(const void* data, size_t len){
        if( len <= 0 )return;
        const char* pointer = (const char*)data;
        //如果正在处于接收数据中, 说明上一个包没有接收完毕
        if(data_bytes){
            //继续接收数据
            if(data_bytes > len){
                //先构造数据
                body.append(pointer, len);
                data_bytes -= len;
                return;
            }
            //如果小于等于
            body.append((const char*)data, data_bytes);
            OnRecvHeaderBody(header, body);
            clear();
            //继续尝试读取
            if( len > data_bytes)return Input((const char*)data + data_bytes, len - data_bytes);
        }
        //如果正在处于接收头中
        else{
            const char *begin = pointer;
            //找到一个http头
            const char *end = strnstr(begin, "\r\n\r\n", len);
            if (end == nullptr)
                return;
            //如果找到了目标
            //拿到头部的长度
            size_t head_length = end - begin;
            //构造头部的长度
            header.append(pointer, head_length);
            //移动指针到头部后的数据区
            begin += head_length += 4;
            //确定是否有数据接收并设置
            //重新解析
            auto size = set_body_size();
            if( !size ){
                OnRecvHeaderBody(header, body);
                clear();
            }
            return Input(begin, len - head_length);
        }
    }

    size_t HttpSplitter::set_body_size(){
        auto find_it = header.find("Content-Length: ");
        if(find_it == std::string::npos){
            data_bytes = 0;
            return data_bytes;
        }
        auto end = header.find("\r\n", find_it);
        if( end == std::string::npos){
            end = header.size() - 1;
        }
        try{
            data_bytes = std::stoi(header.substr(find_it + 16, end - find_it));
        } catch (const std::exception& e) {
            ErrorL << "Content-Length解析错误";
            throw e;
        }
        return data_bytes;
    }
}