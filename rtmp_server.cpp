// rtmp_server.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "srs_kernel_log.hpp"
#include <stdarg.h>
#include <vector>
using namespace std;

#include "NetServer.h"

class CSrsLog :public ISrsLog
{
public:
    CSrsLog()
	{

	}
    virtual ~CSrsLog()
	{
	
	}
public:
    /**
    * initialize log utilities.
    */
	virtual int initialize()
	{
		return 0;
	}
#define  srs_printf(fmt) 	{ char buffer[4096]; va_list args;	va_start(args,fmt);	vsprintf(buffer,fmt,args);	va_end(args);printf(buffer); printf("\r\n"); }
public:
    /**
    * log for verbose, very verbose information.
    */
    virtual void verbose(const char* tag, int context_id, const char* fmt, ...)
	{
		srs_printf(fmt);
	}
    /**
    * log for debug, detail information.
    */
    virtual void info(const char* tag, int context_id, const char* fmt, ...)
	{
		srs_printf(fmt);
	}
    /**
    * log for trace, important information.
    */
    virtual void trace(const char* tag, int context_id, const char* fmt, ...)
	{
		srs_printf(fmt);
	}
    /**
    * log for warn, warn is something should take attention, but not a error.
    */
    virtual void warn(const char* tag, int context_id, const char* fmt, ...)
	{
		srs_printf(fmt);
	}
    /**
    * log for error, something error occur, do something about the error, ie. close the connection,
    * but we will donot abort the program.
    */
    virtual void error(const char* tag, int context_id, const char* fmt, ...)
	{
		srs_printf(fmt);
	}
};





ISrsLog* _srs_log = new CSrsLog();
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

