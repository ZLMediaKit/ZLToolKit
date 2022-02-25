/*
 * @file_name: http_base.hpp
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
#ifndef HTTP_IMPL_HPP
#define HTTP_IMPL_HPP
#include "HttpHeaderBase.hpp"
#include <map>
#include <memory>
#include <string>
namespace http {

  template <bool, typename T, typename R> class basic_message;
  template <typename _header_type, typename _body_type> class http_message_base {
  public:
    using header_type = _header_type;
    using field_type = typename header_type::field_type;
    using value_type = typename header_type::value_type;
    using body_type = _body_type;

  public:
    http_message_base():_keep_alive_(false), _version("HTTP/1.1"){}
    http_message_base(const http_message_base<header_type, body_type> &) = delete;
    http_message_base &operator=(const http_message_base<header_type, body_type>&) = delete;
    http_message_base(http_message_base<_header_type, body_type> &&other)
        : _header_(std::move(other.header)), _body_(std::move(other.header)) {}
    header_type& headers(){ return _header_;}
    body_type &body() { return _body_; }

    const value_type& get_header(const field_type &field) const {
      static value_type null_header;
      auto it = _header_.find(field);
      if (it != _header_.end())
        return (*it).second;
      return null_header;
    }

    void set_header(const field_type &field, const value_type &val) {
      _header_[field] = val;
    }

    bool has_header(const field_type &field) const {
      auto it = _header_.find(field);
      if (it != _header_.end())
        return true;
      return false;
    }


    void keep_alive(bool is_keep){
      _keep_alive_ = is_keep;
    }

    bool keep_alive() const{
      return _keep_alive_;
    }

    const std::string& version() const {
        return _version;
    }

    std::string to_string(){
        return std::move(_header_.to_string());
    }

  private:
    header_type _header_;
    body_type _body_;
    bool _keep_alive_;
    std::string _version;
};
}; // namespace http
#endif