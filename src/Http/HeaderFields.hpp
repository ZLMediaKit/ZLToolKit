/*
 * @file_name: header_fields.hpp
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
#ifndef HEADER_FIELDS_HPP
#define HEADER_FIELDS_HPP
#include "HttpHeaderBase.hpp"
#define REQUEST_HEADER_FIELDS(name, val) \
  class name :public basic_request_header{ \
  public: static constexpr const char* value = val;};
#define RESPONSE_HEADER_FIELDS(name, val) \
  class name :public basic_response_header{ \
  public: static constexpr const char* value = val; };
namespace http{
  class basic_request_header: public std::true_type{};
  class basic_response_header: public std::true_type{};
  class http_request_header : public http_header_base{
  public:
    REQUEST_HEADER_FIELDS(accept, "Accept")
    REQUEST_HEADER_FIELDS(accept_charset, "Accept-Charset")
    REQUEST_HEADER_FIELDS(accept_encoding, "Accept-Encoding")
    REQUEST_HEADER_FIELDS(host, "Host")
    REQUEST_HEADER_FIELDS(authorization, "Authorization")
    REQUEST_HEADER_FIELDS(connection, "Connection")
  };

  class http_response_header: public http_header_base{
  public:
    RESPONSE_HEADER_FIELDS(accept_range, "Accept-Ranges")
    RESPONSE_HEADER_FIELDS(server, "Server")
    RESPONSE_HEADER_FIELDS(allow, "Allow")
    RESPONSE_HEADER_FIELDS(content_encoding, "Content-Encoding")
    RESPONSE_HEADER_FIELDS(content_language, "Content-Language")
    RESPONSE_HEADER_FIELDS(content_length, "Content-Length")
    RESPONSE_HEADER_FIELDS(content_type, "Content-Type")
  };

  using request_field = http_request_header;
  using response_field = http_response_header;
  template<typename T> struct is_response_field: public std::false_type{};
  template<typename T> struct is_request_field: public std::false_type{};

#define IS_REQUEST_FIELD(class_name) template<> struct is_request_field<typename request_field::class_name>: public std::true_type{};
  IS_REQUEST_FIELD(accept)
  IS_REQUEST_FIELD(accept_charset)
  IS_REQUEST_FIELD(accept_encoding)
  IS_REQUEST_FIELD(host)
  IS_REQUEST_FIELD(authorization)
  IS_REQUEST_FIELD(connection)
#define IS_RESPONSE_FIELD(class_name) template<> struct is_response_field<typename response_field::class_name>:public std::true_type{};
  IS_RESPONSE_FIELD(accept_range)
  IS_RESPONSE_FIELD(server)
  IS_RESPONSE_FIELD(allow)
  IS_RESPONSE_FIELD(content_encoding)
  IS_RESPONSE_FIELD(content_language)
  IS_RESPONSE_FIELD(content_length)
  IS_RESPONSE_FIELD(content_type)
};
#endif // VIDEO_ON_DEMAND_HEADER_FIELDS_HPP
