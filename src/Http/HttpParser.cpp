/*
* @file_name: HttpParser.cpp
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
#include "HttpParser.hpp"
#include "Util/util.h"
namespace http{
    void HttpParser::CreateHeader(std::string& raw_header,
                                       std::string& method,
                                       std::string& version,
                                       std::string& path, std::map<std::string, std::string>& header){
        std::vector<std::string> content = std::move(toolkit::split(raw_header, "\r\n"));
        if (!content.size())
            throw std::logic_error("错误的http请求: 解析头部字段失败");

        //解析请求方法
        std::smatch cm;
        size_t cursor = std::string::npos;
        auto& request_line = content[0];
        //匹配正则表达
        auto ret = std::regex_match(request_line, cm, reg);
        if(!ret && cm.size() != 4)
            throw std::logic_error("http解析错误: 错误的请求方式<Method> <Path> <Version>");

        //拿到方法
        method = std::move(cm[1].str());
        toolkit::strToUpper(method);
        //请求路径
        path = std::move(cm[2].str());
        //http
        version = std::move(cm[3].str());
        toolkit::strToUpper(version);

        for(int i = 1; i < content.size(); i++){

            auto& request_header = content[i];
            auto index = request_header.find(": ");
            if( index == std::string::npos){
                throw std::logic_error("http头解析错误");
            }
            //构造field
            std::string header_field = std::move(request_header.substr(0, index));
            //构造值
            header[header_field] = std::move(request_header.substr(index + 2));
        }
    }

    void HttpParser::ParseArgs(const std::string& src, std::map<std::string,std::string>& attr_map, const char* delim, const char* key_delim){
        auto lines = std::move(toolkit::split(src, delim));
        for( const auto& line : lines){
            auto delim_index = line.find(key_delim);
            if( delim_index != std::string::npos){
                std::string field = line.substr(0, delim_index);
                std::string value = line.substr(delim_index + 1);
                attr_map[field] = std::move(value);
            }else{
                attr_map[line] = line;
            }
        }
    }

}