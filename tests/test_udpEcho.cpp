//
// Created by 沈昊 on 2021/12/17.
//
#include <Network/UdpServer.h>
#include <Util/logger.h>
using namespace toolkit;
int main(){
  Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
  Logger::Instance().add(std::make_shared<ConsoleChannel>());


  return 0;
}