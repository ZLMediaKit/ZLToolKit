/*
 * @file_name:http_status.hpp
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
#ifndef HTTP_STATUS_HPP
#define HTTP_STATUS_HPP
#define HTTP_BASE_STATUS(name, code, desciption)                               \
  class name : public std::integral_constant<int, code>{                       \
  public:                                                                      \
      using native_handle_type = int;                                          \
  public:                                                                      \
    static constexpr const char *desciptor = desciption;                       \
    static constexpr const char *long_descriptor = desciption;};


#define HTTP_STATUS(name, code, desciption, means)                              \
  class name : public std::integral_constant<int, code>{                        \
  public:                                                                       \
    static constexpr const char *desciptor = desciption;                        \
    static constexpr const char *long_descriptor = means;};

#include <type_traits>
namespace http {
namespace status {
  /* RFC page 38 */
  /* Client Error 4xx*/
  HTTP_STATUS(ok, 200, "OK", "The request has succeeded.")
  HTTP_STATUS(bad_request, 400,"Bad Request", "The request could not be understood by the server due to malformed syntax.")
  HTTP_STATUS(unauthorized, 401, "Unauthorized", "The request requires user authentication.")
  HTTP_STATUS(forbidden, 403, "Forbidden", "The server understood the request, but is refusing to fulfill it.")
  HTTP_STATUS(not_found,  404, "Not Found", "The server has not found anything matching the Request-URI.")
  HTTP_STATUS(method_not_allowed, 405, "Method not Allowed", "The method specified in the Request-Line is not allowed for the resource identified by the Request-URI.")
  HTTP_STATUS(request_time_out, 408, "Request Timeout", "The client did not produce a request within the time that the server was prepared to wait.")
  HTTP_STATUS(length_required, 411, "Length Required", "The server refuses to accept the request without a defined Content-Length.")
  HTTP_STATUS(entity_too_long, 413, "Request Entity Too Large", "The server is refusing to process a request because the request entity is larger than the server is willing or able to process.")
  HTTP_STATUS(unsupported_media_type, 415, "Unsupported Media Type", "The server is refusing to service the request because the entity of the request is in a format not supported by the requested resource for the requested method.")
  /* Server Error 5xx */
  HTTP_STATUS(interanl_server_error, 500, "Internal Server Error", "The server encountered an unexpected condition which prevented it from fulfilling the request.")
  HTTP_STATUS(not_implemented, 501, "Not Implemented", "The server does not support the functionality required to fulfill the request.")
  HTTP_STATUS(version_not_supported, 505, "Version Not Supported", "The server does not support, or refuses to support, the HTTP protocol version that was used in the request message.")

  }; // namespace status

  template<typename T> struct is_http_status: public std::false_type{};
#define IS_HTTP_STATUS(name) template<> struct is_http_status<typename http::status::name>: public std::true_type{};
  IS_HTTP_STATUS(ok)
  IS_HTTP_STATUS(bad_request)
  IS_HTTP_STATUS(unauthorized)
  IS_HTTP_STATUS(forbidden)
  IS_HTTP_STATUS(not_found)
  IS_HTTP_STATUS(method_not_allowed)
  IS_HTTP_STATUS(request_time_out)
  IS_HTTP_STATUS(length_required)
  IS_HTTP_STATUS(entity_too_long)
  IS_HTTP_STATUS(unsupported_media_type)
  IS_HTTP_STATUS(interanl_server_error)
  IS_HTTP_STATUS(not_implemented)
  IS_HTTP_STATUS(version_not_supported)

  template<typename T, typename = typename std::enable_if<http::is_http_status<T>::value>::type>
  struct http_status{
    static constexpr const int code = T::value;
    static constexpr const char* desc = T::desciptor;
    static constexpr const char* long_desc = T::long_descriptor;
  };
}; // namespace http

#endif // VIDEO_ON_DEMAND_STATUS_HPP
