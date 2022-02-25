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
#ifndef HTTP_JSON_BODY_HPP
#define HTTP_JSON_BODY_HPP
#include <json/json.h>
namespace http {

  class json_parser {
  public:
    using value_type = typename Json::Value;

  public:
    static void serialize(const value_type &val, std::string &target) {
      Json::FastWriter write_builder;
      target = write_builder.write(val);
    }
    static void from_string(const std::string &source, value_type &val) {
      Json::CharReaderBuilder read_builder;
      std::string err;
      std::unique_ptr<Json::CharReader> reader(read_builder.newCharReader());
      if (!reader)
        throw std::bad_alloc();
      if (!reader->parse(source.data(), source.data() + source.size(), &val,
                         &err)) {
        throw std::logic_error(err);
      }
    }
  };

  class json_body {
  public:
    static constexpr const char *content_type = "application/json";
    using parser_type = typename http::json_parser;
    using value_type = typename http::json_parser::value_type;
    using size_type = size_t;

  public:
    json_body() {}
    json_body(const json_body &) = delete;
    json_body &operator=(const json_body &) = delete;
    const std::string& to_string() {
      return val;
    }

    value_type &operator[](const char *key) { return value[key]; }

    void flush(){
        parser_type::serialize(value, val);
    }

    size_t size()const{
        return val.size();
    }
  public:
    bool empty() const { return value.size() == 0; }

  private:
    std::string val;
    value_type value;
  };
}; // namespace http

#endif