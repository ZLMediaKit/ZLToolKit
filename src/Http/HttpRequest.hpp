/*
 * @file_name: http_request_impl.hpp
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
#ifndef HTTP_REQUEST_IMPL_HPP
#define HTTP_REQUEST_IMPL_HPP
#include "HeaderFields.hpp"
#include "HttpBase.hpp"
#include "HttpMethod.hpp"
namespace http{
    template<typename _body_type>
    class basic_message<true, http_request_header, _body_type>: protected http_message_base<http_request_header, _body_type>,
                        public request_field{
    public:
        friend class HttpSession;
    public:
      using base_header_type = http_message_base<http_request_header, _body_type>;
      using header_type = typename base_header_type::header_type;
      using body_type = typename base_header_type::body_type;
      using value_type = typename base_header_type::value_type;

    public:
      header_type& headers(){ return base_header_type::headers();}
      body_type &body() { return base_header_type::body(); }

      template<typename T, typename = typename std::enable_if<http::is_request_field<T>::value>::type>
      const value_type& get_header(){
        return base_header_type::get_header(T::value);
      }

      template<typename T, typename R, typename = typename std::enable_if<http::is_request_field<T>::value>::type>
      void set_header(const R& val){
        return base_header_type::set_header(T::value, std::move(std::to_string(val)));
      }

      template<typename T, typename = typename std::enable_if<http::is_request_field<T>::value>::type>
      bool has_header(){
        return base_header_type::has_header(T::value);
      }

      template<typename T, typename = typename std::enable_if<http::is_http_method<T>::value>::type>
      void method(){
          _method = T::name;
      }

      const std::string& method()const{
          return _method;
      }


      std::string to_string(){
          std::string str = std::move(base_header_type::to_string());
          str += body().to_string();
          return std::move(str);
      }

  private:
      std::string _method;
    };

    template<typename body> using http_request = basic_message<true, http_request_header, body>;
};
#endif

