/*
* @file_name:rfc822_datetime_format.hpp
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
#ifndef SHTOOLKIT_RFC822_DATETIME_HPP
#define SHTOOLKIT_RFC822_DATETIME_HPP
#include <string>

static const char* s_month[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char* s_week[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char* s_zone[] = {
    "UTC", "GMT", "EST", "EDT", "CST", "CDT",
    "MST", "MDT", "PST", "PDT"
};


namespace dateformat{
  class rfc_datetime_format_exception : public std::logic_error{
  public:
    explicit rfc_datetime_format_exception(const string& s): std::logic_error(s){}
    explicit rfc_datetime_format_exception(const char* str): std::logic_error(str){}
  };


template<typename clockType>
  std::string rfc822_datetime_format(const typename::clockType::time_point& point){
    int r = 0;
    time_t time = point.to_time_t();
    struct tm *tm = gmtime(&time);
    char buff[256] = {0};
    r = snprintf(buff, sizeof(buff), "%s, %02d %s %04d %02d:%02d:%02d GMT",
                 s_week[(unsigned int)tm->tm_wday % 7],
                 tm->tm_mday,
                 s_month[(unsigned int)tm->tm_mon % 12],
                 tm->tm_year+1900,
                 tm->tm_hour,
                 tm->tm_min,
                 tm->tm_sec);
    if( r <= 0 || r >= 30){
      throw rfc_datetime_format_exception("不符合rfc822规范");
    }
    return std::string(buff, r);
  }
}
#endif // SHTOOLKIT_RFC822_DATETIME_HPP
