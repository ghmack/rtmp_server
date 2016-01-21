// rtmp_server.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "kernel/srs_kernel_log.hpp"
#include <stdarg.h>
#include <vector>
using namespace std;

#include "NetServer.h"


ISrsLog* _srs_log = new ISrsLog();
ISrsThreadContext* _srs_context = new ISrsThreadContext();

void ThrowException1(const char* exp,...) 
{ 
	char buffer[1000],buffer2[2000];
	va_list args;
	va_start(args,exp);
	vsprintf(buffer,exp,args);
	va_end(args);
	sprintf(buffer2,"%s,%s,%d\r\n",buffer,__FILE__,__LINE__);
	printf(buffer2);
	//throw buffer;
}

int _tmain(int argc, _TCHAR* argv[])
{
	//ThrowException1("1234,%d,%s",3,"2");
	//ThrowException1("1234,");
	vector<char> c;
	char buffer[3] = {0,1,2};
	c.insert(c.end(),buffer,buffer+3);

	RtmpNetServer server("0.0.0.0",1935);
	server.start();

	return 0;
}

