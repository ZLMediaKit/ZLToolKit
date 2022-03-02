//
// Created by 沈昊 on 2022/2/26.
//
#include <Util/string_view.hpp>
#include <Util/logger.h>
using namespace toolkit;
void test_1(){
  toolkit::string_view view("shenhao");
  //string_view.substr对比std::string的substr，可以取消std::string的内部一次内存申请开销
  InfoL << view;
  InfoL << view.find_first_of("ehoae");
  InfoL << view.find("hao");
  view.remove_prefix(2);
  InfoL << view.find_first_of("ehoae");
  InfoL << view.substr(0, 1);
  InfoL << view.substr(0, 2);
  InfoL << view.substr(0, 3);
  InfoL << view.substr(0, 4);
}

void test_split(){
  toolkit::string_view view("GET /index HTTP/1.1");
  //这里构造string_view也更有效率
  auto vec = view.split(" ");
  for(const auto& item : vec){
    InfoL << item;
  }
}

void test_map(){
  ///构造map时可以减少内存拷贝，只要request的生命周期与map绑定
  std::string request = "Host: localhost:8080\r\nConnection: keep-alive\r\n\r\n";
  std::map<toolkit::string_view, toolkit::string_view> _map = {
      {{request.data(), 4},{request.data() + 6, 14}},
      {{request.data() + 22, 12}, {request.data() + 34, 10}}
  };
  for(const auto& item : _map){
    InfoL << item.first << " " << item.second;
  }
}
int main(){
  Logger::Instance().add(std::make_shared<ConsoleChannel>());
  test_1();
  test_split();
  test_map();
  return 0;
}