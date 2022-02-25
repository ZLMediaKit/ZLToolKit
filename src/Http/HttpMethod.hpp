/*
 * @file_name: http_method.hpp
 * @date: 2021/12/11
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
#ifndef HTTP_METHOD_HPP
#define HTTP_METHOD_HPP
#include <type_traits>
/*
 * Method Definitions
 * the set of common method for HTTP/1.1 is defined below, Although this set can
 * be expanded. The Host request-header field MUST accompany all HTTP/1.1
 * requests.
 * */
namespace http{
  namespace method {
#define METHOD(NAME, DEC) \
  struct NAME {       \
      static constexpr const char* name = DEC; };
  METHOD(get, "GET")
  METHOD(post, "POST")
  METHOD(put, "PUT")
  METHOD(del, "DELETE")
  };
  template<typename T> struct is_http_method: public std::false_type{};
#define IS_HTTP_METHOD(name) template<> struct is_http_method<typename http::method::name>: public std::true_type{};
  IS_HTTP_METHOD(get)
  IS_HTTP_METHOD(post)
  IS_HTTP_METHOD(put)
  IS_HTTP_METHOD(del)

  template<typename T, typename = typename std::enable_if<http::is_http_method<T>::value>::type>
  struct HttpMethod {
    static constexpr const char* desc = T::name;
  };

};
#endif // VIDEO_ON_DEMAND_METHOD_HPP
