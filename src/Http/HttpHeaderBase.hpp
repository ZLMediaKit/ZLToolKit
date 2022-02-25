/*
 * @file_name: http_header_base.hpp
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
#ifndef HTTP_HEADER_HPP
#define HTTP_HEADER_HPP
#include <map>
namespace http{

  class http_header_base: public std::map<std::string, std::string>{
  public:
    using field_type = typename std::map<std::string, std::string>::key_type;
    using value_type = typename std::map<std::string, std::string>::mapped_type;
    using base_type = std::map<field_type, value_type>;
  public:
    http_header_base(http_header_base&& other):base_type(std::move(other)){
    }
    http_header_base() {}
    http_header_base& operator = (http_header_base&& other){
        base_type::operator=(std::move(other));
        return *this;
    }
public:
    bool has_field(const field_type& field){
      auto it = base_type::find(field);
      return it == base_type::end();
    }

    value_type& operator[](const field_type& field){
        return base_type::operator[](field);
    }

    std::string to_string(){
        std::string str;
        auto begin = base_type::begin();
        auto end = base_type::end();
        while(begin != end){
            str += (*begin).first + ": " + (*begin).second + "\r\n";
            ++begin;
        }
        return std::move(str);
    }
  };
};

#endif // VIDEO_ON_DEMAND_HTTP_HEADER_HPP
