// homer::Url v0.3.0
// MIT License
// https://github.com/homer6/url

// This class takes inspiration and some source code from
// https://github.com/chriskohlhoff/urdl/blob/master/include/urdl/url.hpp

#pragma once
#ifndef URI_HPP
#define URI_HPP
#if defined(__clang__) || defined(__GNUC__)
  #define CPP_STANDARD __cplusplus
#elif defined(_MSC_VER)
  #define CPP_STANDARD _MSVC_LANG
#endif

#ifdef __cplusplus
  #if CPP_STANDARD >= 201703L
    #include <string_view>
    using string_view = typename std::string_view;
  #else
    #include "string_view.hpp"
    using string_view = toolkit::string_view;
  #endif
#endif


#include <string>
#include <map>

/*
    Url and UrlView are compliant with
        https://tools.ietf.org/html/rfc3986
        https://tools.ietf.org/html/rfc6874
        https://tools.ietf.org/html/rfc7320
        and adheres to https://rosettacode.org/wiki/URL_parser examples.

    Url will use default ports for known schemes, if the port is not explicitly provided.
*/


class Url{

public:

  Url() = default;
  explicit Url( const std::string& s );

  const std::string& getScheme() const;
  const std::string& getUsername() const;
  const std::string& getPassword() const;
  const std::string& getHost() const;
  unsigned short getPort() const;
  const std::string& getPath();
  const std::string& getQuery() const;
  const std::multimap<std::string,std::string>& getQueryParameters() const;
  const std::string& getParameter(const char*) const;
  const std::string& getFragment() const;


  void fromString( const std::string& s );

  friend bool operator==(const Url& a, const Url& b);
  friend bool operator!=(const Url& a, const Url& b);
  friend bool operator<(const Url& a, const Url& b);

  void setSecure( bool secure );

  bool isIpv6() const;
  bool isSecure() const;

  std::string toString() const;
  explicit operator std::string() const;

protected:

  static bool unescape_path(const std::string& in, std::string& out);

  string_view captureUpTo( const string_view& right_delimiter, const std::string& error_message = "" );
  bool moveBefore( const string_view& right_delimiter );
  bool existsForward( const string_view& right_delimiter );

  std::string scheme;
  std::string authority;
  std::string user_info;
  std::string username;
  std::string password;
  std::string host;
  std::string port;
  std::string path;
  std::string query;
  std::multimap<std::string,std::string> query_parameters;
  std::string fragment;
  std::string tmp_path;
  bool secure = false;
  bool ipv6_host = false;
  bool authority_present = false;

  std::string whole_url_storage;
  size_t left_position = 0;
  size_t right_position = 0;
  string_view parse_target;

};
#endif