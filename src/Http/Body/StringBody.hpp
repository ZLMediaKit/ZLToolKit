/*
 * @file_name: string_body.hpp
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
#ifndef STRING_BODY_HPP
#define STRING_BODY_HPP
#include "AnyBody.hpp"
#include <cstring>
#include <memory>
#include <string>
namespace http {
class string_body : protected std::basic_string<char> {
public:
  static constexpr const char *content_type = "text/plain";
  using parser_type = std::string;
  using size_type = typename std::basic_string<char>::size_type;
  using value_type = std::basic_string<char>;
  using Ptr = std::shared_ptr<string_body>;

public:
  string_body() = default;

  string_body(const char *str) : value_type(str, strlen(str)) {
    const char *s = str;
  }

  template <size_t len>
  string_body(const char arr[len]) : value_type(arr, len - 1) {}

  string_body(const value_type &val) : value_type(val) {}

  string_body &append(const char *body, size_t len) {
    value_type::append(body, len);
    return *this;
  }

  string_body &append(const string_body &other) {
    value_type::append(other);
    return *this;
  }

  size_t size() const{
      return std::string::size();
  }

  string_body& operator = (const std::string& str){
      std::string::operator=(str);
      return *this;
  }

  string_body& operator = (std::string&& str){
      std::string::operator=(std::move(str));
      return *this;
  }

  void flush(){}
public:
  value_type to_string() const {
    value_type _this(*this);
    return std::move(_this);
  }

  bool empty() { return value_type::empty(); }
};
}; // namespace http

#endif