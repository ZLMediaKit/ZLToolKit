/*
 * @file_name: http_response_impl.hpp
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
#ifndef HTTP_RESPONSE_IMPL_HPP
#define HTTP_RESPONSE_IMPL_HPP
#include "HeaderFields.hpp"
#include "HttpBase.hpp"
#include "HttpStatus.hpp"
#include <string>
namespace http {

  template <typename _body_type>
  class basic_message<false, http_response_header, _body_type> : protected http_message_base<http_response_header, _body_type> {
  public:
    using base_header_type = http_message_base<http_response_header, _body_type>;
    using header_type = typename base_header_type::header_type;
    using body_type = typename base_header_type::body_type;
    using value_type = typename base_header_type::value_type;
  public:
    header_type& headers(){ return base_header_type::headers();}
    body_type &body()  { return base_header_type::body(); }

    template<typename T, typename = typename std::enable_if<http::is_response_field<T>::value>::type>
    const value_type& get_header(){
      return base_header_type::get_header(T::value);
    }

    template<typename T, typename R, typename = typename std::enable_if<http::is_response_field<T>::value>::type>
    void set_header(const R& val){
      return base_header_type::set_header(T::value, std::move(std::to_string(val)));
    }

    template<typename T, typename = typename std::enable_if<http::is_response_field<T>::value>::type>
    void set_header(const char* val){
        return base_header_type::set_header(T::value, val);
    }

    template<typename T, typename = typename std::enable_if<http::is_response_field<T>::value>::type>
    bool has_header(){
      return base_header_type::has_header(T::value);
    }


    unsigned int status() const {
        return status_code;
    }

    template<typename T, typename = typename std::enable_if<http::is_http_status<T>::value>::type>
    void status(){
        status_code = (unsigned int)T::value;
        status_descrip = T::desciptor;
    }

    std::string to_string() {
        body().flush();
        std::string str = "HTTP/1.1 ";
        //状态
        str += std::to_string(status_code) + " ";
        str += status_descrip + "\r\n";
        set_header<http::http_response_header::content_length>(body().size());
        set_header<http::http_response_header::content_type>(_body_type::content_type);
        str += std::move(base_header_type::to_string());
        str += "\r\n";
        str += body().to_string();
        return std::move(str);
    }
   private:
      unsigned int status_code;
      std::string status_descrip;
  };
  template <typename T> using http_response = basic_message<false, http_response_header, T>;
}; // namespace http
#endif