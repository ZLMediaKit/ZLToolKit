# 一个基于C++11简单易用的轻量级网络编程框架
[![Build Status](https://travis-ci.org/xiongziliang/ZLToolKit.svg?branch=master)](https://travis-ci.org/xiongziliang/ZLToolKit)

## 项目特点
- 基于C++11开发，避免使用裸指针，代码稳定可靠；同时跨平台移植简单方便，代码清晰简洁。
- 使用epoll+线程池+异步网络IO模式开发，并发性能优越。
- 代码经过大量的稳定性、性能测试，可满足商用服务器项目。
- 支持linux、macos、ios、android、windows平台
- 了解更多:[ZLMediaKit](https://github.com/xiongziliang/ZLMediaKit)

## 特性
- 网络库
  - tcp/udp客户端，接口简单易用并且是线程安全的，用户不必关心具体的socket api操作。
  - tcp服务器，使用非常简单，只要实现具体的tcp会话（TcpSession类）逻辑,使用模板的方式可以快速的构建高性能的服务器。
  - 对套接字多种操作的封装。
- 线程库
  - 使用线程实现的简单易用的定时器。
  - 信号量。
  - 线程组。
  - 简单易用的线程池，可以异步或同步执行任务，支持functional 和 lambad表达式。
- 工具库
  - 文件操作。
  - std::cout风格的日志库，支持颜色高亮、代码定位、异步打印。
  - INI配置文件的读写。
  - 监听者模式的消息广播器。
  - 基于智能指针的循环池，不需要显式手动释放。
  - 环形缓冲，支持主动读取和读取事件两种模式。
  - mysql链接池，使用占位符（？）方式生成sql语句，支持同步异步操作。
  - 简单易用的ssl加解密黑盒，支持多线程。
  - 其他一些有用的工具。
  - 命令行解析工具，可以很便捷的实现可配置应用程序

## 编译(Linux)
- 我的编译环境
  - Ubuntu16.04 64 bit + gcc5.4(最低gcc4.7)
  - cmake 3.5.1
- 编译

  ```
  cd ZLToolKit
  ./build_for_linux.sh
  ```  
  
## 编译(macOS)
- 我的编译环境
  - macOS Sierra(10.12.1) + xcode8.3.1
  - Homebrew 1.1.3
  - cmake 3.8.0
- 编译
  
  ```
  cd ZLToolKit
  ./build_for_mac.sh
  ```
	 
## 编译(iOS)
- 编译环境:`请参考macOS的编译指导。`
- 编译
  
  ```
  cd ZLToolKit
  ./build_for_ios.sh
  ```
- 你也可以生成Xcode工程再编译：

  ```
  cd ZLToolKit
  mkdir -p build
  cd build
  # 生成Xcode工程，工程文件在build目录下
  cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/iOS.cmake -DIOS_PLATFORM=SIMULATOR64 -G "Xcode"
  ```
## 编译(Android)
- 我的编译环境
  - macOS Sierra(10.12.1) + xcode8.3.1
  - Homebrew 1.1.3
  - cmake 3.8.0
  - [android-ndk-r14b](https://dl.google.com/android/repository/android-ndk-r14b-darwin-x86_64.zip)
- 编译

  ```
  cd ZLToolKit
  export ANDROID_NDK_ROOT=/path/to/ndk
  ./build_for_android.sh
  ```
## 编译(Windows)
- 我的编译环境
  - windows 10
  - visual studio 2017
  - [openssl](http://slproweb.com/download/Win32OpenSSL-1_1_0f.exe)
  - [mysqlclient](https://dev.mysql.com/downloads/file/?id=472430)
  - [cmake-gui](https://cmake.org/files/v3.10/cmake-3.10.0-rc1-win32-x86.msi)
  
- 编译
```
   1 使用cmake-gui打开工程并生成vs工程文件.
   2 找到工程文件(ZLToolKit.sln),双击用vs2017打开.
   3 选择编译Release 版本.
   4 依次编译 ZLToolKit_static、ZLToolKit_shared、ALL_BUILD、INSTALL.
   5 找到目标文件并运行测试用例.
   6 找到安装的头文件及库文件(在源码所在分区根目录).
```
## 授权协议

本项目自有代码使用宽松的MIT协议，在保留版权信息的情况下可以自由应用于各自商用、非商业的项目。
但是本项目也零碎的使用了一些其他的开源代码，在商用的情况下请自行替代或剔除；
由于使用本项目而产生的商业纠纷或侵权行为一概与本项项目及开发者无关，请自行承担法律风险。

## QA
 - 该库性能怎么样？

基于ZLToolKit，我实现了一个流媒体服务器[ZLMediaKit](https://github.com/xiongziliang/ZLMediaKit);作者已经对其进行了性能测试，可以查看[benchmark.md](https://github.com/xiongziliang/ZLMediaKit/blob/master/benchmark.md)了解详情。

 - 该库稳定性怎么样？

该库经过作者严格的valgrind测试，长时间大负荷的测试；作者也使用该库进行了多个线上项目的开发。实践证明该库稳定性很好；可以无看门狗脚本的方式连续运行几个月。

 - 在windows下编译很多错误？
 
 由于本项目主体代码在macOS/linux下开发，部分源码采用的是无bom头的UTF-8编码；由于windows对于utf-8支持不甚友好，所以如果发现编译错误请先尝试添加bom头再编译。


## 联系方式
- 邮箱：<1213642868@qq.com>（本项目相关或网络编程相关问题请走issue流程，否则恕不邮件答复）
- QQ群：542509000


