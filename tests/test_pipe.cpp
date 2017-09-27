#include <signal.h>
#include <iostream>
#include "Util/logger.h"
#include "Poller/EventPoller.h"
#include "Poller/Pipe.h"
#include "Util/util.h"
using namespace std;
using namespace ZL::Util;
using namespace ZL::Poller;


int main() {
	//设置日志
	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
	Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
#if defined(_WIN32)
	FatalL << "该测试程序不能再windows下运行，因为我不会windows下的多进程编程，但是管道模块是可以在windows下正常工作的。" << endl;
#else
    //获取父进程的PID
	auto parentPid = getpid();
	InfoL << "parent pid:" << parentPid << endl;

	//定义一个管道，lambada类型的参数是管道收到数据的回调
	Pipe pipe([](int size,const char *buf) {
		//该管道有数据可读了
		InfoL << getpid() << " recv:" << buf;
	});

	//创建子进程
	auto pid = fork();

	if (pid == 0) {
		//子进程
		int i = 10;
		while (i--) {
			//在子进程每隔一秒把数据写入管道，共计发送10次
			sleep(1);
			auto msg = StrPrinter << "message " << i << " form subprocess:" << getpid() << endl;
			DebugL << "子进程发送:" << msg << endl;
			pipe.send(msg.data(), msg.size());
		}
		DebugL << "子进程退出" << endl;
	} else {
		//父进程设置退出信号处理函数
		signal(SIGINT, [](int){EventPoller::Instance().shutdown();});
		//父进程开始事件轮询
		EventPoller::Instance().runLoop();
		//轮询退出，说明收到SIGINT信号，程序开始退出
		EventPoller::Destory();
		InfoL << "父进程退出" << endl;
	}
#endif // defined(_WIN32)

	//销毁log
	Logger::Destory();
	return 0;
}
