//============================================================================
// Name        : ToolKitTest.cpp
// Author      : xzl
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include "Util/logger.h"
using namespace std;
using namespace ZL::Util;

int main() {
	Logger::Instance().add(std::make_shared<ConsoleChannel> ("stdout", LTrace));
	Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

	TraceL << "int"<< (int)1  << endl;
	DebugL << "short:"<< (short)2  << endl;
	InfoL << "float:" << (float)3.12345678  << endl;
	WarnL << "double:" << (double)4.12345678901234567  << endl;
	ErrorL << "void *:" << (void *)0x12345678 << endl;
	FatalL << "without endl!";

	Logger::Destory();
	return 0;
}
