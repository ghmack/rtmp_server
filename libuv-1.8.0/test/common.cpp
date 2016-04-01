#include "common.h"


common::common(void)
{
}


common::~common(void)
{
}

LibuvUsageEnvironment::LibuvUsageEnvironment(LibuvTaskScheduler* taskScheduler):m_taskScheduler(taskScheduler)
{
	
}

LibuvUsageEnvironment::~LibuvUsageEnvironment()
{

}


LibuvTaskScheduler* LibuvUsageEnvironment::TaskScheduler()
{
	return m_taskScheduler;
}


//////////////////////////////////////////////////////////////////////////
LibuvTaskScheduler::LibuvTaskScheduler(uv_loop_t* loop):m_uv_loop(loop)
{
	
}

LibuvTaskScheduler::~LibuvTaskScheduler()
{
	if(m_uv_loop)
	{
		uv_loop_close(m_uv_loop);
	}
}

LibuvTaskScheduler* LibuvTaskScheduler::createNew()
{
	uv_loop_t* loop = uv_default_loop();
	if (loop)
	{
		return new LibuvTaskScheduler(loop);
	}
	return NULL;
}

void LibuvTaskScheduler::timer_cb(uv_timer_t* hTimer)
{
	print_info("LibuvTaskScheduler::timer_cb \r\n");
	uv_timer_t* uv_timer = (uv_timer_t*)hTimer;
	task_data* task = (task_data*)uv_timer->data;
	ASSERT(task);
	task->data_cb(task->data_client);
}

TaskToken LibuvTaskScheduler::scheduleDelayedTask(int64_t microseconds, TaskFunc* proc,
	void* clientData)
{
	uv_timer_t* uv_timer = (uv_timer_t*)malloc(sizeof uv_timer_t);
	int ret = uv_timer_init(m_uv_loop, uv_timer);
	ASSERT(ret == 0);

	task_data* task = new task_data;
	task->data_cb = proc;
	task->data_client = clientData;
	uv_timer->data = task;

	ret = uv_timer_start(uv_timer, LibuvTaskScheduler::timer_cb, microseconds,0);

	ASSERT(ret == 0);


	return uv_timer;
}

 void LibuvTaskScheduler::timer_close_cb(uv_handle_t* timer)
 {
	 if(timer)
	 free(timer);
 }

void LibuvTaskScheduler::unscheduleDelayedTask(TaskToken& prevTask)
{
	uv_timer_t* uv_timer = (uv_timer_t*)prevTask;
	uv_timer_stop(uv_timer);
	if (uv_timer->data)
	{
		delete uv_timer->data;
		uv_timer->data = NULL;
	}
	uv_close((uv_handle_t*)uv_timer,timer_close_cb);
	free(uv_timer);
}


void LibuvTaskScheduler::rescheduleDelayedTask(TaskToken& task,
	int64_t microseconds, TaskFunc* proc,
	void* clientData)
{
	uv_timer_t* uv_timer = (uv_timer_t*)task;
	uv_timer_stop(uv_timer);
	task_data* task_cb = (task_data*)uv_timer->data;
	ASSERT(task_cb);
	task_cb->data_cb = proc;
	task_cb->data_client = clientData;
	int ret = uv_timer_start(uv_timer, LibuvTaskScheduler::timer_cb, microseconds,0);
	//uv_timer_again(uv_timer);
	ASSERT(ret == 0);
}


void LibuvTaskScheduler::doEventLoop()
{
	uv_run(m_uv_loop,UV_RUN_DEFAULT);
}


void* LibuvTaskScheduler::loopHandle()
{
	return m_uv_loop;
}

//////////////////////////////////////////////////////////////////////////

UvTcpSocket* UvTcpSocket::createUvTcpSokcet(
	LibuvUsageEnvironment* env, 
	string szAddr, 
	int port)
{
	int ret = 0;
	do 
	{
		uv_loop_t* uv_loop = (uv_loop_t*)env->TaskScheduler()->loopHandle();
		ASSERT(uv_loop);
		if(!uv_loop){
			print_info("create loop error\r\n");
			ret = -1;
			break;
		}

		struct sockaddr_in addr;

		ret = uv_ip4_addr(szAddr.c_str(),port,&addr);
		ASSERT(ret == 0);
		if (ret){
			print_info("get ip error\r\n");
			break;
		}

		uv_tcp_t* tcp_stream = (uv_tcp_t*)malloc(sizeof uv_tcp_t);
		ret = uv_tcp_init(uv_loop, tcp_stream);
		ASSERT(ret == 0);
		if(ret){
			print_info("create server socket error\r\n");
			break;
		}

		ret = uv_tcp_bind(tcp_stream,(const sockaddr*)&addr,0);
		if (ret)
		{
			print_info("bind error \r\n");
			break;
		}

		return new UvTcpSocket(env, tcp_stream);
	}while(0);

	return NULL;
}

UvTcpSocket::UvTcpSocket(LibuvUsageEnvironment* env,uv_tcp_t* uv_tcp)
	:m_env(env),m_uv_tcp(uv_tcp),m_uv_connect(NULL),m_uv_write(NULL),
	m_accepted_cb(NULL),m_accepted_cb_param(NULL),
	m_connected_cb(NULL),m_connected_cb_param(NULL),
	m_recv_cb(NULL),m_recv_cb_param(NULL),
	m_write_cb(NULL),m_write_cb_param(NULL),
	m_buffer(NULL),m_bufferSize(0)
{
	
}
UvTcpSocket::~UvTcpSocket()
{
	if(m_uv_connect)
	{
		free(m_uv_connect);
	}
	if (m_uv_write)
	{
		free(m_uv_write);
	}

	if (m_uv_tcp)
	{
		uv_close((uv_handle_t*)m_uv_tcp,UvTcpSocket::on_close);
	}

}

 void UvTcpSocket::on_close(uv_handle_t* handle)
 {
	 if (handle)
	 {
		 free(handle);
	 }
 }


int UvTcpSocket::asyncAccept(on_accepted_cb cb,void* cbParam)
{
	int ret = 0;
	m_accepted_cb = cb;
	m_accepted_cb_param = cbParam;
	do 
	{
		m_uv_tcp->data = this;
		ret = uv_listen((uv_stream_t*)m_uv_tcp,5,UvTcpSocket::on_connection);
		if (ret)
		{
			print_info("listen error\r\n");
			break;
		}

	} while (0);
	return ret ;
}

void UvTcpSocket::on_connection(uv_stream_t* stream, int status)
{
	UvTcpSocket* pThis = reinterpret_cast<UvTcpSocket*> (stream->data);
	pThis->on_connection1(stream, status);
}

void UvTcpSocket::on_connection1(uv_stream_t* stream, int status)
{
	uv_stream_t* client = NULL;
	do 
	{
		ASSERT(status == 0);
		if (status)
		{
			print_info("connection error\r\n");
			break;
		}

		client = (uv_stream_t*)malloc(sizeof uv_tcp_t);
		int ret  = uv_tcp_init((uv_loop_t*)m_env->TaskScheduler()->loopHandle(), (uv_tcp_t*)client);
		ASSERT(ret == 0);
		if (ret)
		{
			print_info("tcp error \r\n");
			break;
		}

		ret = uv_accept(stream,(uv_stream_t*)client);
		ASSERT(ret == 0);
		if(ret)
		{
			print_info("accept error\r\n");
			break;
		}

		UvTcpSocket* clientSocket = new UvTcpSocket(m_env,(uv_tcp_t*)client);

		if(m_accepted_cb)
			m_accepted_cb(clientSocket,status,m_accepted_cb_param);

		return ;

	} while (0);

	if(client)
		free(client);
}


int  UvTcpSocket::asyncConnect(string ip,int port,on_connected cb,void* cbParam)
{
	m_connected_cb = cb;
	m_connected_cb_param = cbParam;
	int ret  = 0;
	sockaddr_in sockAddr;
	ret = uv_ip4_addr(ip.c_str(),port,&sockAddr);

	if(m_uv_connect)
	{
		free(m_uv_connect);
		m_uv_connect = (uv_connect_t*)malloc(sizeof uv_connect_t);
	}
	m_uv_connect->data = this;
	return uv_tcp_connect(m_uv_connect, m_uv_tcp,(const sockaddr*)&sockAddr,UvTcpSocket::on_connect);

}


void UvTcpSocket::on_connect(uv_connect_t* conn, int status)
{
	UvTcpSocket* pThis = reinterpret_cast<UvTcpSocket*> (conn->data);
	pThis->on_connect1(conn, status);
}

void UvTcpSocket::on_connect1(uv_connect_t* conn, int status)
{
	ASSERT(status == 0);
	if(m_connected_cb)
	{
		m_connected_cb(status,m_connected_cb_param);
	}
}


void UvTcpSocket::on_alloc_recv(uv_handle_t* handle,size_t suggestSize, uv_buf_t* buf)
{
	UvTcpSocket* pThis = reinterpret_cast<UvTcpSocket*> (handle->data);
	pThis->on_alloc_recv1(handle,suggestSize,buf);

}

void UvTcpSocket::on_alloc_recv1(uv_handle_t* handle,size_t suggestSize, uv_buf_t* buf)
{
	buf->base = (char*)m_buffer;
	buf->len = m_bufferSize;
}

void UvTcpSocket::on_read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
	UvTcpSocket* pThis = reinterpret_cast<UvTcpSocket*> (stream->data);
	pThis->on_read_cb1(stream,nread,buf);
}
void UvTcpSocket::on_read_cb1(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
	if(nread == 0)
		return ;
	int readSize = nread;

	if(m_recv_cb)
		m_recv_cb(readSize,m_recv_cb_param);
}


void UvTcpSocket::resetReadCB(on_recv_cb cb,void* cbParam)
{
	m_recv_cb = cb;
	m_recv_cb_param = cbParam;
}



int	 UvTcpSocket::asyncReadStart(void* buffer, int size,on_recv_cb cb,void* cbParam)
{
	m_recv_cb = cb;
	m_recv_cb_param = cbParam;
	m_buffer = buffer;
	m_bufferSize = size;

	m_uv_tcp->data = this;
	return uv_read_start((uv_stream_t*)m_uv_tcp, UvTcpSocket::on_alloc_recv, UvTcpSocket::on_read_cb);
}



int  UvTcpSocket::asyncWrite(const uv_buf_t data[], int count, on_write_cb cb, void* cbParam)
{
	m_write_cb = cb;
	m_write_cb_param = cbParam;

	if(!m_uv_write)
	{
		m_uv_write = (uv_write_t*)malloc(sizeof uv_write_t);
		
	}
	m_uv_write->data = this;
	return uv_write(m_uv_write,(uv_stream_t*) m_uv_tcp, data, count, UvTcpSocket::on_write);
	//return uv_write(m_uv_write,(uv_stream_t*) m_uv_tcp, &buf, count, UvTcpSocket::on_write);
}


void UvTcpSocket::on_write(uv_write_t* req_t, int status)
{
	UvTcpSocket* pThis = reinterpret_cast<UvTcpSocket*> (req_t->data);
	pThis->on_write1(req_t,status);
}


void UvTcpSocket::on_write1(uv_write_t* req_t, int status)
{
	if (m_write_cb)
	{
		m_write_cb(status,m_write_cb_param);
	}
}