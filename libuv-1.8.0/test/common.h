#pragma once

#include "uv.h"
#include <stdio.h>
#include <assert.h>
#include <string>
using namespace std;

#define print_info printf
#define ASSERT  assert


class common
{
public:
	common(void);
	~common(void);
};

class LibuvTaskScheduler;



class LibuvUsageEnvironment
{
public:
	LibuvUsageEnvironment(LibuvTaskScheduler* taskScheduler);
	virtual ~LibuvUsageEnvironment();

	LibuvTaskScheduler* TaskScheduler();

private:
	LibuvTaskScheduler* m_taskScheduler;

};


typedef void TaskFunc(void* clientData);
typedef void* TaskToken;



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


class UvTcpSocket;

typedef void (*on_accepted_cb)(UvTcpSocket* connection, int status,void* param);
typedef void (*on_connected)(int status,void* param);
typedef void (*on_recv_cb)(int recvSize,void* param);
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

class UvTcpSocket
{
public:
	static UvTcpSocket* createUvTcpSokcet(
		LibuvUsageEnvironment* env,
		string addr,
		int port);

	virtual ~UvTcpSocket();

	int  asyncAccept(on_accepted_cb cb,void* cbParam);
	int  asyncConnect(string ip,int port,on_connected cb,void* cbParam);

	void resetReadCB(on_recv_cb cb,void* cbParam);
	int	 asyncReadStart(void* buffer, int size,on_recv_cb cb,void* cbParam);
	int  asyncWrite(const uv_buf_t data[], int count, on_write_cb cb, void* cbParam);

protected:
	UvTcpSocket(LibuvUsageEnvironment* env,uv_tcp_t* uv_tcp);

	static void on_connection(uv_stream_t*, int status);
	void on_connection1(uv_stream_t*, int status);

	static void on_connect(uv_connect_t* conn, int status);
	void on_connect1(uv_connect_t* conn, int status);

	static void on_alloc_recv(uv_handle_t* handle,size_t suggestSize, uv_buf_t* buf);
	void on_alloc_recv1(uv_handle_t* handle,size_t suggestSize, uv_buf_t* buf);

	static void on_read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
	void on_read_cb1(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);

	static void on_write(uv_write_t* req_t, int status);
	void on_write1(uv_write_t* req_t, int status);

	static void on_close(uv_handle_t* handle);

private:
	LibuvUsageEnvironment* m_env;
	uv_tcp_t* m_uv_tcp;
	on_accepted_cb m_accepted_cb;
	void* m_accepted_cb_param;
	on_connected m_connected_cb;
	void* m_connected_cb_param;
	on_recv_cb m_recv_cb;
	void* m_recv_cb_param;
	on_write_cb m_write_cb;
	void* m_write_cb_param;
	void* m_buffer;
	int	  m_bufferSize;
	uv_connect_t* m_uv_connect;
	uv_write_t* m_uv_write;
};



class UvUdpSocket
{
public:
	UvUdpSocket* createUvUdpSocket(LibuvUsageEnvironment* en, string ip, int port);
	~UvUdpSocket();
protected :
	UvUdpSocket(LibuvUsageEnvironment* en,string ip,int port);
	int asyncReadStart()

};



