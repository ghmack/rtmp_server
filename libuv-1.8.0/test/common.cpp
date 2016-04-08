#include "common.h"




LibuvUsageEnvironment::LibuvUsageEnvironment(LibuvTaskScheduler* taskScheduler):m_taskScheduler(taskScheduler)
{
	m_iLevel = 0;
	m_logBufferSize = 4096;
	m_logBuffer = (char*)malloc(m_logBufferSize);
}

LibuvUsageEnvironment::~LibuvUsageEnvironment()
{

}


LibuvTaskScheduler* LibuvUsageEnvironment::TaskScheduler()
{
	return m_taskScheduler;
}

void LibuvUsageEnvironment::printMsg(int ilevle, string szLog, ...)
{
	if (iLevel >= m_iLevel)
	{
		va_list valist;
		va_start(valist,szLog);
		fprintf(stderr,szLog,valist);
		va_end(valist);
	}
	fprintf(stderr,"\r\n");
	return ;

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


///UvSocket
//////////////////////////////////////////////////////////////////////////
UvSocket::UvSocket(LibuvUsageEnvironment* env):m_env(env),
	m_recv_cb(NULL),m_recv_cb_param(NULL),
	m_write_cb(NULL),m_write_cb_param(NULL)
{

}


UvSocket::~UvSocket()
{

}

int  UvSocket::asyncAccept(on_accepted_cb cb,void* cbParam)
{
	return 0;
}
int  UvSocket::asyncConnect(string ip,int port,on_connected cb,void* cbParam)
{
	return 0;
}

int  UvSocket::setMuticastLoop(bool bLoop)
{
	return 0;
}
int  UvSocket::setMuticastTTL(int newTTL)
{
	return 0;
}
int  UvSocket::joinMuticastGroup(string muticastAddr,string interfaceAddr)
{
	return 0;
}
int  UvSocket::leaveMuticastGroup(string muticastAddr,string interfaceAddr)
{
	return 0;
}
int  UvSocket::setMuticastInterface(string interfaceAddr)
{
	return 0;
}
int  UvSocket::setBroadcast(bool bOn)
{
	return 0;
}
int  UvSocket::setTTL(int newTTL)
{
	return 0;
}


void UvSocket::resetReadCB(on_recv_cb cb,void* cbParam)
{
	m_recv_cb = cb;
	m_recv_cb_param = cbParam;
}

void UvSocket::on_alloc_recv(uv_handle_t* handle,size_t suggestSize, uv_buf_t* buf)
{
	UvTcpSocket* pThis = reinterpret_cast<UvTcpSocket*> (handle->data);
	pThis->on_alloc_recv1(handle,suggestSize,buf);

}

void UvSocket::on_alloc_recv1(uv_handle_t* handle,size_t suggestSize, uv_buf_t* buf)
{
	buf->base = (char*)m_buffer;
	buf->len = m_bufferSize;
}

void UvSocket::on_close(uv_handle_t* handle)
{
	if (handle)
	{
		free(handle);
	}
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
	:UvSocket(env),m_uv_tcp(uv_tcp),m_uv_connect(NULL),m_uv_write(NULL),
	m_accepted_cb(NULL),m_accepted_cb_param(NULL),
	m_connected_cb(NULL),m_connected_cb_param(NULL)
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
		uv_read_stop((uv_stream_t*)m_uv_tcp);
		uv_close((uv_handle_t*)m_uv_tcp,UvSocket::on_close);
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
		m_recv_cb(readSize,m_recv_cb_param,NULL,0);
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



int  UvTcpSocket::asyncWrite(const uv_buf_t data[], int count, on_write_cb cb, void* cbParam, const struct sockaddr* addr /*= NULL*/)
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
	ASSERT(pThis);
	pThis->on_write1(req_t,status);
}


void UvTcpSocket::on_write1(uv_write_t* req_t, int status)
{
	if (m_write_cb)
	{
		m_write_cb(status,m_write_cb_param);
	}
}


//////////////////////////////////////////////////////////////////////////


UvUdpSocket* UvUdpSocket::createUvUdpSocket(LibuvUsageEnvironment* env, string ip, int port)
{
	uv_loop_t* uv_loop = (uv_loop_t*)env->TaskScheduler()->loopHandle();
	int ret = 0;
	do 
	{
		ASSERT(uv_loop);
		if(!uv_loop){
			print_info("create loop error\r\n");
			ret = -1;
			break;
		}
		uv_udp_t* uv_udp = (uv_udp_t*)malloc(sizeof uv_udp_t);
		ret = uv_udp_init(uv_loop, uv_udp);

		struct sockaddr_in addrIn;
		ret = uv_ip4_addr(ip.c_str(),port, &addrIn);
		if(ret != 0)
		{
			break;
		}
		ret = uv_udp_bind(uv_udp, (const sockaddr*)&addrIn, 0);
		if(ret != 0)
		{
			break;
		}

		return new UvUdpSocket(env,uv_udp);

	} while (0);

	return NULL;

}


UvUdpSocket::UvUdpSocket(LibuvUsageEnvironment* env,uv_udp_t* udp)
	:UvSocket(env),m_uv_udp(udp),m_udp_req(NULL)
{

}

UvUdpSocket::~UvUdpSocket()
{
	if (m_udp_req)
	{
		free(m_udp_req);
	}
	if (m_uv_udp)
	{
		uv_udp_recv_stop(m_uv_udp);
		uv_close((uv_handle_t*)m_uv_udp,UvSocket::on_close);

	}
}


int UvUdpSocket::asyncReadStart(void* buffer, int size,on_recv_cb cb,void* cbParam)
{
	m_recv_cb = cb;
	m_recv_cb_param = cbParam;
	m_buffer = buffer;
	m_bufferSize = size;

	m_uv_udp->data = this;
	return uv_udp_recv_start( m_uv_udp, UvSocket::on_alloc_recv, UvUdpSocket::udp_recv_cb);

}


void UvUdpSocket::udp_recv_cb(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags)
{
	UvUdpSocket* pThis = reinterpret_cast<UvUdpSocket*>(handle->data);
	if (pThis)
	{
		pThis->udp_recv_cb1(handle,nread,buf,addr,flags);
	}
}
void UvUdpSocket::udp_recv_cb1(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags)
{
	if (m_recv_cb)
	{
		m_recv_cb(nread, m_recv_cb_param, addr,flags);
	}
}

int UvUdpSocket::asyncWrite(const uv_buf_t data[], int count, on_write_cb cb, void* cbParam, const struct sockaddr* addr )
{
	m_write_cb = cb;
	m_write_cb_param = cbParam;

	if (!m_udp_req)
	{
		m_udp_req = (uv_udp_send_t*)malloc(sizeof uv_udp_send_t);
	}
	m_udp_req->data = this;
	return uv_udp_send(m_udp_req,m_uv_udp, data, count,addr,UvUdpSocket::udp_send_cb);

}

void UvUdpSocket::udp_send_cb(uv_udp_send_t* req, int status)
{
	UvUdpSocket* pthis = reinterpret_cast<UvUdpSocket*>(req->data);
	if (pthis)
	{
		pthis->udp_send_cb1(req,status);
	}
}

void UvUdpSocket::udp_send_cb1(uv_udp_send_t* req, int status)
{
	if (m_write_cb)
	{
		m_write_cb(status, m_write_cb_param);
	}
}

int  UvUdpSocket::setMuticastLoop(bool bLoop)
{
	return uv_udp_set_multicast_loop(m_uv_udp,bLoop?1:0);
}
int  UvUdpSocket::setMuticastTTL(int newTTL)
{
	return uv_udp_set_multicast_ttl(m_uv_udp,newTTL);
}
int  UvUdpSocket::joinMuticastGroup(string muticastAddr,string interfaceAddr)
{
	return uv_udp_set_membership(m_uv_udp, muticastAddr.c_str(), interfaceAddr.c_str(), UV_JOIN_GROUP);
}
int  UvUdpSocket::leaveMuticastGroup(string muticastAddr,string interfaceAddr)
{
	return uv_udp_set_membership(m_uv_udp, muticastAddr.c_str(), interfaceAddr.c_str(), UV_LEAVE_GROUP);
}
int  UvUdpSocket::setMuticastInterface(string interfaceAddr)
{
	return uv_udp_set_multicast_interface(m_uv_udp,interfaceAddr.c_str());
}
int  UvUdpSocket::setBroadcast(bool bOn)
{
	return uv_udp_set_broadcast(m_uv_udp, bOn?1:0);
}
int  UvUdpSocket::setTTL(int newTTL)
{
	return uv_udp_set_ttl(m_uv_udp,newTTL);
}