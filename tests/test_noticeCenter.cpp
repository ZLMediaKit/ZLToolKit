//============================================================================
// Name        : ToolKitTest.cpp
// Author      : xzl
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================
#include <signal.h>
#include <unistd.h>
#include "Util/logger.h"
#include "Util/NoticeCenter.h"
using namespace std;
using namespace ZL::Util;

#define NOTICE_NAME1 "NOTICE_NAME1"
#define NOTICE_NAME2 "NOTICE_NAME2"
bool g_bExitFlag = false;

void programExit(int arg) {
	g_bExitFlag = true;
}
int main() {
	signal(SIGINT, programExit);
	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
	NoticeCenter::Instance().addListener(0,NOTICE_NAME1,
			[](int a,const char *b,double c,string &d){
		DebugL << a << " " << b << " " << c << " " << d;
	});

	NoticeCenter::Instance().addListener(0,NOTICE_NAME2,
			[](string &d,double c,const char *b,int a){
		DebugL << a << " " << b << " " << c << " " << d;
	});
	int a = 0;
	while(!g_bExitFlag){
		const char *b = "b";
		double c = 3.14;
		string d("d");
		NoticeCenter::Instance().emitEvent(NOTICE_NAME1,++a,(const char *)"b",c,d);
		NoticeCenter::Instance().emitEvent(NOTICE_NAME2,d,c,b,a);
		sleep(1); // sleep 1 ms
	}
	Logger::Destory();
	return 0;
}
