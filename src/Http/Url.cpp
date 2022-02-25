// homer::Url v0.3.0
// MIT License
// https://github.com/homer6/url

#include "Url.hpp"

#include <cstring>
#include <cctype>
#include <cstdlib>
#include <algorithm>
#include <stdexcept>


Url::Url(){

}


Url::Url( const string& s ){
  this->fromString(s);
}



string Url::getScheme() const{
  return this->scheme;
}



string Url::getUsername() const{
  return this->username;
}



string Url::getPassword() const{
  return this->password;
}



string Url::getHost() const{
  return this->host;
}



unsigned short Url::getPort() const{

  if( this->port.size() > 0 ){
    return std::atoi( this->port.c_str() );
  }

  if( this->scheme == "https" ) return 443;
  if( this->scheme == "http" ) return 80;
  if( this->scheme == "ssh" ) return 22;
  if( this->scheme == "ftp" ) return 21;
  if( this->scheme == "mysql" ) return 3306;
  if( this->scheme == "mongo" ) return 27017;
  if( this->scheme == "mongo+srv" ) return 27017;
  if( this->scheme == "kafka" ) return 9092;
  if( this->scheme == "postgres" ) return 5432;
  if( this->scheme == "postgresql" ) return 5432;
  if( this->scheme == "redis" ) return 6379;
  if( this->scheme == "zookeeper" ) return 2181;
  if( this->scheme == "ldap" ) return 389;
  if( this->scheme == "ldaps" ) return 636;
  if( this->scheme == "rtsp") return 554;
  if( this->scheme == "rtmp") return 1935;
  return 0;

}



string Url::getPath() const{

  std::string tmp_path;
  unescape_path( this->path, tmp_path );
  return tmp_path;

}



string Url::getQuery() const{
  return this->query;
}



const multimap<string,string>& Url::getQueryParameters() const{
  return this->query_parameters;
}



string Url::getFragment() const{
  return this->fragment;
}


bool Url::isIpv6() const{
  return this->ipv6_host;
}


void Url::setSecure( bool secure_ ){
  this->secure = secure_;
}

bool Url::isSecure() const{
  return this->secure;
}







string_view Url::captureUpTo( const string_view right_delimiter, const string& error_message ){

  this->right_position = this->parse_target.find_first_of( right_delimiter, this->left_position );

  if( right_position == std::string::npos && error_message.size() ){
    throw std::runtime_error(error_message);
  }

  string_view captured = this->parse_target.substr( this->left_position, this->right_position - this->left_position );

  return captured;

}


bool Url::moveBefore( const string_view right_delimiter ){

  size_t position = this->parse_target.find_first_of( right_delimiter, this->left_position );

  if( position != std::string::npos ){
    this->left_position = position;
    return true;
  }

  return false;

}

bool Url::existsForward( const string_view right_delimiter ){

  size_t position = this->parse_target.find_first_of( right_delimiter, this->left_position );

  if( position != std::string::npos ){
    return true;
  }

  return false;

}




void Url::fromString( const std::string& source_string ){

  this->whole_url_storage = source_string;  //copy


  //reset target
  this->parse_target = this->whole_url_storage;
  this->left_position = 0;
  this->right_position = 0;


  this->authority_present = false;


  // scheme
  this->scheme = this->captureUpTo( ":", "Expected : in Url" );
  std::transform(
      this->scheme.begin(), this->scheme.end(),
      this->scheme.begin(), []( string_view::value_type c){ return std::tolower(c); }
  );
  this->left_position += scheme.size() + 1;


  // authority

  if( this->moveBefore( "//" ) ){
    this->authority_present = true;
    this->left_position += 2;
  }

  if( this->authority_present ){

    this->authority = this->captureUpTo( "/" );

    bool path_exists = false;

    if( this->moveBefore( "/" ) ){
      path_exists = true;
    }

    if( this->existsForward("?") ){

      this->path = this->captureUpTo( "?" );
      this->moveBefore("?");
      this->left_position++;

      if( this->existsForward("#") ){
        this->query = this->captureUpTo( "#" );
        this->moveBefore("#");
        this->left_position++;
        this->fragment = this->captureUpTo( "#" );
      }else{
        //no fragment
        this->query = this->captureUpTo( "#" );
      }

    }else{

      //no query
      if( this->existsForward("#") ){
        this->path = this->captureUpTo( "#" );
        this->moveBefore("#");
        this->left_position++;
        this->fragment = this->captureUpTo( "#" );
      }else{
        //no fragment
        if( path_exists ){
          this->path = this->captureUpTo( "#" );
        }

      }

    }

  }else{

    this->path = this->captureUpTo( "#" );

  }



  //parse authority


  //reset target
  this->parse_target = this->authority;
  this->left_position = 0;
  this->right_position = 0;


  if( this->existsForward("@") ){

    this->user_info = this->captureUpTo( "@" );
    this->moveBefore("@");
    this->left_position++;

  }else{
    //no user_info

  }

  //detect ipv6
  if( this->existsForward("[") ){
    this->left_position++;
    this->host = this->captureUpTo( "]", "Malformed ipv6" );
    this->left_position++;
    this->ipv6_host = true;
  }else{

    if( this->existsForward(":") ){
      this->host = this->captureUpTo( ":" );
      this->moveBefore(":");
      this->left_position++;
      this->port = this->captureUpTo( "#" );
    }else{
      //no port
      this->host = this->captureUpTo( ":" );
    }

  }


  //parse user_info

  //reset target
  this->parse_target = this->user_info;
  this->left_position = 0;
  this->right_position = 0;


  if( this->existsForward(":") ){

    this->username = this->captureUpTo( ":" );
    this->moveBefore(":");
    this->left_position++;

    this->password = this->captureUpTo( "#" );

  }else{
    //no password

    this->username = this->captureUpTo( ":" );

  }


  //update secure
  if( this->scheme == "ssh" || this->scheme == "https" || this->port == "443" ){
    this->secure = true;
  }

  if( this->scheme == "postgres" || this->scheme == "postgresql" ){

    //reset parse target to query
    this->parse_target = this->query;
    this->left_position = 0;
    this->right_position = 0;

    if( this->existsForward("ssl=true") ){
      this->secure = true;
    }

  }



}



bool Url::unescape_path( const std::string& in, std::string& out ){

  out.clear();
  out.reserve( in.size() );

  for( std::size_t i = 0; i < in.size(); ++i ){

    switch( in[i] ){

    case '%':

      if( i + 3 <= in.size() ){

        unsigned int value = 0;

        for( std::size_t j = i + 1; j < i + 3; ++j ){

          switch( in[j] ){

          case '0': case '1': case '2': case '3': case '4':
          case '5': case '6': case '7': case '8': case '9':
            value += in[j] - '0';
            break;

          case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
            value += in[j] - 'a' + 10;
            break;

          case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            value += in[j] - 'A' + 10;
            break;

          default:
            return false;
          }

          if( j == i + 1 ) value <<= 4;

        }


        out += static_cast<char>(value);
        i += 2;

      }else{

        return false;

      }

      break;



    case '-': case '_': case '.': case '!': case '~': case '*':
    case '\'': case '(': case ')': case ':': case '@': case '&':
    case '=': case '+': case '$': case ',': case '/': case ';':
      out += in[i];
      break;


    default:
      if( !std::isalnum(in[i]) ) return false;
      out += in[i];
      break;

    }

  }

  return true;

}



bool operator==( const Url& a, const Url& b ){

  return a.scheme == b.scheme
         && a.username == b.username
         && a.password == b.password
         && a.host == b.host
         && a.port == b.port
         && a.path == b.path
         && a.query == b.query
         && a.fragment == b.fragment;

}



bool operator!=( const Url& a, const Url& b ){

  return !(a == b);

}



bool operator<( const Url& a, const Url& b ){

  if( a.scheme < b.scheme ) return true;
  if( b.scheme < a.scheme ) return false;

  if( a.username < b.username ) return true;
  if( b.username < a.username ) return false;

  if( a.password < b.password ) return true;
  if( b.password < a.password ) return false;

  if( a.host < b.host ) return true;
  if( b.host < a.host ) return false;

  if( a.port < b.port ) return true;
  if( b.port < a.port ) return false;

  if( a.path < b.path ) return true;
  if( b.path < a.path ) return false;

  if( a.query < b.query ) return true;
  if( b.query < a.query ) return false;

  return a.fragment < b.fragment;

}



string Url::toString() const{

  return this->whole_url_storage;

}


Url::operator string() const{
  return this->toString();
}









