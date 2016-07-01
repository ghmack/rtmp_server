#pragma once

#include "uv.h"
#include <stdio.h>
#include <assert.h>
#include <string>
using namespace std;

#define print_info printf
#define LOG_DEBUG(s,...) printf(s"\r\n",##__VA_ARGS__);
#define LOG_INFO(s,...)  printf(s"\r\n",##__VA_ARGS__);
#define LOG_WARN(s,...)  printf(s"\r\n",##__VA_ARGS__);
#define LOG_ERROR(s,...) printf(s"\r\n",##__VA_ARGS__);
#define LOG_FATAL(s,...) printf(s"\r\n",##__VA_ARGS__);
#define ASSERT  assert

class UvSocket;
class UvTcpSocket;



#define LOG_LEVEL_ALL   0
#define LOG_LEVEL_DEBUG 1
#define LOG_LEVEL_INFO  2
#define LOG_LEVEL_WARN  3
#define LOG_LEVEL_ERROR 4
#define LOG_LEVEL_FATAL 5

class LibuvTaskScheduler;



class LibuvUsageEnvironment
{
public:
	LibuvUsageEnvironment(LibuvTaskScheduler* taskScheduler);
	virtual ~LibuvUsageEnvironment();

	virtual LibuvTaskScheduler* TaskScheduler();
	virtual void printMsg(int ilevle, string szLog, ...);

private:
	LibuvTaskScheduler* m_taskScheduler;

};




typedef void TaskFunc(void* clientData);
typedef void* TaskToken;
typedef void BackgroundHandlerProc(void* clientData, int mask);


class LibuvTaskScheduler
{
protected:
	LibuvTaskScheduler();
	LibuvTaskScheduler(uv_loop_t* loop);
	typedef struct task_data
	{
		TaskFunc* data_cb;
		void* data_client;
	};
public:
	static LibuvTaskScheduler* createNew();
	virtual ~LibuvTaskScheduler();

	
	// Possible bits to set in "mask".  (These are deliberately defined
	// the same as those in Tcl, to make a Tcl-based subclass easy.)
	#define SOCKET_READABLE    (1<<1)
	#define SOCKET_WRITABLE    (1<<2)
	#define SOCKET_EXCEPTION   (1<<3)
	//virtual void setBackgroundHandling(int socketNum, int conditionSet, BackgroundHandlerProc* handlerProc, void* clientData) ;
	//void disableBackgroundHandling(int socketNum) { setBackgroundHandling(socketNum, 0, NULL, NULL); }
	//virtual void moveSocketHandling(int oldSocketNum, int newSocketNum) ;
	virtual void setBackgroundHandling(UvSocket* socketNum, int conditionSet, BackgroundHandlerProc* handlerProc, void* clientData) ;
	void disableBackgroundHandling(UvSocket* socketNum) { setBackgroundHandling(socketNum, 0, NULL, NULL); }
	virtual void moveSocketHandling(UvSocket* oldSocketNum, UvSocket* newSocketNum) ;

	virtual TaskToken scheduleDelayedTask(int64_t microseconds, TaskFunc* proc,
		void* clientData);

	virtual void unscheduleDelayedTask(TaskToken& prevTask);

	virtual void rescheduleDelayedTask(TaskToken& task,
		int64_t microseconds, TaskFunc* proc,
		void* clientData);

	virtual void doEventLoop();

	void* loopHandle();
protected:
	static void timer_cb(uv_timer_t* hTimer);
	static void timer_close_cb(uv_handle_t* timer);

private:
	uv_loop_t* m_uv_loop;
};




typedef void (*on_accepted_cb)(UvTcpSocket* connection, int status,void* param);
typedef void (*on_connected)(int status,void* param);
typedef void (*on_recv_cb)(int recvSize,void* param,const struct sockaddr* addr, unsigned flags);
typedef void (*on_write_cb)(int status,void* param);

//#if defined(_WIN32) 
//typedef struct uv_buf_t {
//	ULONG len;
//	char* base;
//} uv_buf_t;
//#else
//typedef struct uv_buf_t {
//	char* base;
//	size_t len;
//} uv_buf_t;
//#endif

class  UvSocket
{
public:
	static int inetAddr(const char* ip, int port, struct sockaddr_in* addr);
	static int inetString(const struct sockaddr_in* src, char* dst, size_t size);
public:
	UvSocket(LibuvUsageEnvironment* env);
	virtual ~UvSocket();

	virtual int  asyncAccept();
	virtual int  asyncConnect(string ip,int port);

	virtual int	 asyncReadStart(void* buffer, int size) =0 ;
	virtual int  asyncWrite(const uv_buf_t data[], int count,  const struct sockaddr* addr /*= NULL*/) = 0;
	
	virtual int  getSockname(struct sockaddr_in* name) =0;
	virtual int  getPeername(struct sockaddr_in* name) ;

	virtual int  setMuticastLoop(bool bLoop);
	virtual int  setMuticastTTL(int newTTL);
	virtual int  joinMuticastGroup(string muticastAddr,string interfaceAddr);
	virtual int  leaveMuticastGroup(string muticastAddr,string interfaceAddr);
	virtual int  setMuticastInterface(string interfaceAddr);
	virtual int  setBroadcast(bool bOn);
	virtual int  setTTL(int newTTL);

	virtual int  nodelay(bool enable);
	virtual int  keepAlive(bool enable, unsigned int value);
	virtual int  simultaneousAccept(bool enable);

	void assignBackgroundHandling(BackgroundHandlerProc* handlerProc, void* clientData,int conditionSet);
	int  status();
	unsigned  flags();
	UvSocket* newConnection();
	int readSize();

protected:
	static void on_alloc_recv(uv_handle_t* handle,size_t suggestSize, uv_buf_t* buf);
	void on_alloc_recv1(uv_handle_t* handle,size_t suggestSize, uv_buf_t* buf);

	static void on_close(uv_handle_t* handle);
protected:
	LibuvUsageEnvironment* m_env;
	void* m_buffer;
	int	  m_bufferSize;

	int m_status;	
	unsigned m_flags;
	UvSocket* m_newConnection;
	

	int m_readSize;

	BackgroundHandlerProc* m_ioHandlerProc;
	void* m_ClientData;

	//BackgroundHandlerProc* m_writeHandlerProc;
	//void* m_writeClientData;



};





class UvTcpSocket :public UvSocket
{
public:
	static UvTcpSocket* createUvTcpSokcet(
		LibuvUsageEnvironment* env,
		string addr,
		int port);

	virtual ~UvTcpSocket();

	virtual int  asyncAccept();
	virtual int  asyncConnect(string ip,int port);

	virtual int	 asyncReadStart(void* buffer, int size);
	virtual int  asyncWrite(const uv_buf_t data[], int count, const struct sockaddr* addr = NULL);
	virtual int  getSockname(struct sockaddr_in* name);
	virtual int  getPeername(struct sockaddr_in* name);

	virtual int  nodelay(bool enable);
	virtual int  keepAlive(bool enable, unsigned int value);
	virtual int  simultaneousAccept(bool enable);

protected:
	UvTcpSocket(LibuvUsageEnvironment* env,uv_tcp_t* uv_tcp);

	static void on_connection(uv_stream_t*, int status);
	void on_connection1(uv_stream_t*, int status);

	static void on_connect(uv_connect_t* conn, int status);
	void on_connect1(uv_connect_t* conn, int status);


	static void on_read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
	void on_read_cb1(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);

	static void on_write(uv_write_t* req_t, int status);
	void on_write1(uv_write_t* req_t, int status);

	

protected:

	uv_tcp_t* m_uv_tcp;

	uv_connect_t* m_uv_connect;
	uv_write_t* m_uv_write;
	
};





class UvUdpSocket:public UvSocket
{
public:
	static UvUdpSocket* createUvUdpSocket(LibuvUsageEnvironment* en, string ip, int port);
	~UvUdpSocket();
protected :
	UvUdpSocket(LibuvUsageEnvironment* en,uv_udp_t* udp);
public:
	virtual int	 asyncReadStart(void* buffer, int size);
	virtual int  asyncWrite(const uv_buf_t data[], int count,  const struct sockaddr* addr /*= NULL*/);

	virtual int  setMuticastLoop(bool bLoop);
	virtual int  setMuticastTTL(int newTTL);
	virtual int  joinMuticastGroup(string muticastAddr,string interfaceAddr);
	virtual int  leaveMuticastGroup(string muticastAddr,string interfaceAddr);
	virtual int  setMuticastInterface(string interfaceAddr);
	virtual int  setBroadcast(bool bOn);
	virtual int  setTTL(int newTTL);

	virtual int  getSockname(struct sockaddr_in* name);
	virtual int  getPeername(struct sockaddr_in* name);
	
protected:
	static void udp_send_cb(uv_udp_send_t* req, int status);
	void udp_send_cb1(uv_udp_send_t* req, int status);

	static void udp_recv_cb(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags);
	void udp_recv_cb1(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags);
protected:


	uv_udp_t* m_uv_udp;
	uv_udp_send_t* m_udp_req;
	struct sockaddr_in m_lastSockaddr;
};

