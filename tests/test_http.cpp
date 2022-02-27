//
// Created by 沈昊 on 2022/2/25.
//
#include <string_view>
#include <map>
#include <iostream>
#include <Util/string_view.hpp>
int main(){

  std::map<std::string_view , std::string_view> _map = {
    {"123", "456"}
  };
  auto it = _map.find("123");
  if(it != _map.end()){
    std::cout << it->second << std::endl;
  }
  //constexpr const char* content_length = "Content-Length: ";
  //constexpr size_t length = std::char_traits<char>::length(content_length);

  std::string str;


  return 0;
}