//
// Created by 沈昊 on 2022/2/25.
//
#include <Util/Url.hpp>
#include <Util/logger.h>
#include <Util/string_view.hpp>
using namespace toolkit;
int main(){
  Logger::Instance().add(std::make_shared<ConsoleChannel>());
  Url url{ "https://fred:password@www.wikipedia.org/what-me-worry/path?hello=there#wonder"};

  std::cout << url.getScheme()   << std::endl;		// https
  std::cout << url.getUsername() << std::endl;		// fred
  std::cout << url.getPassword() << std::endl;		// password
  std::cout << url.getHost()     << std::endl;		// www.wikipedia.org
  std::cout << url.getPort()     << std::endl;	// 443
  std::cout << url.getPath()     << std::endl;		// /what-me-worry
  std::cout << url.getQuery()    << std::endl;		// hello=there
  std::cout << url.getFragment() << std::endl;		// wonder
  std::cout << url.isSecure()    << std::endl;		// bool(true)
  std::cout << url.isIpv6()      << std::endl;		// bool(false)

  toolkit::string_view view{"data"};
  std::cout << view;

  return 0;
}