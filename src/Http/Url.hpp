// homer::Url v0.3.0
// MIT License
// https://github.com/homer6/url

// This class takes inspiration and some source code from
// https://github.com/chriskohlhoff/urdl/blob/master/include/urdl/url.hpp

#pragma once

#include <string>
using std::string;

#include <string_view>
using std::string_view;

#include <map>
using std::multimap;

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

  Url();
  Url( const std::string& s );

  string getScheme() const;
  string getUsername() const;
  string getPassword() const;
  string getHost() const;
  unsigned short getPort() const;
  string getPath() const;
  string getQuery() const;
  const multimap<string,string>& getQueryParameters() const;
  string getFragment() const;


  void fromString( const std::string& s );

  friend bool operator==(const Url& a, const Url& b);
  friend bool operator!=(const Url& a, const Url& b);
  friend bool operator<(const Url& a, const Url& b);

  void setSecure( bool secure );

  bool isIpv6() const;
  bool isSecure() const;

  string toString() const;
  explicit operator string() const;


protected:

  static bool unescape_path(const std::string& in, std::string& out);

  string_view captureUpTo( const string_view right_delimiter, const string& error_message = "" );
  bool moveBefore( const string_view right_delimiter );
  bool existsForward( const string_view right_delimiter );

  string scheme;
  string authority;
  string user_info;
  string username;
  string password;
  string host;
  string port;
  string path;
  string query;
  multimap<string,string> query_parameters;
  string fragment;

  bool secure = false;
  bool ipv6_host = false;
  bool authority_present = false;

  string whole_url_storage;
  size_t left_position = 0;
  size_t right_position = 0;
  string_view parse_target;

};