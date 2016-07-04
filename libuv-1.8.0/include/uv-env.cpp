#include "uv-env.h"




LibuvUsageEnvironment::LibuvUsageEnvironment(LibuvTaskScheduler* taskScheduler):m_taskScheduler(taskScheduler)
{
	//m_iLevel = 0;
	//m_logBufferSize = 4096;
	//m_logBuffer = (char*)malloc(m_logBufferSize);
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
	//if (iLevel >= m_iLevel)
	//{
	//	va_list valist;
	//	va_start(valist,szLog);
	//	fprintf(stderr,szLog,valist);
	//	va_end(valist);
	//}
	//fprintf(stderr,"\r\n");
	//return ;

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



void LibuvTaskScheduler::setBackgroundHandling(UvSocket* socketNum, int conditionSet, BackgroundHandlerProc* handlerProc, void* clientData)
{
	socketNum->assignBackgroundHandling(handlerProc,clientData,conditionSet);
}

void LibuvTaskScheduler::moveSocketHandling(UvSocket* oldSocketNum, UvSocket* newSocketNum)
{

}

void LibuvTaskScheduler::timer_cb(uv_timer_t* hTimer)
{
	LOG_DEBUG("LibuvTaskScheduler::timer_cb \r\n");
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
UvSocket::UvSocket(LibuvUsageEnvironment* env)
	:m_env(env),m_ioHandlerProc(NULL),m_ClientData(NULL),
	m_readSize(0),m_buffer(NULL),m_bufferSize(0)
{

}


UvSocket::~UvSocket()
{

}


unsigned  UvSocket::flags()
{
	return m_flags;
}

UvSocket* UvSocket::newConnection()
{
	return m_newConnection;
}

int UvSocket::readSize()
{
	return m_readSize;
}

void UvSocket::assignBackgroundHandling(BackgroundHandlerProc* handlerProc, void* clientData,int conditionSet)
{

	m_ioHandlerProc= handlerProc;
	m_ClientData = clientData;

}

int UvSocket::inetAddr(const char* ip, int port, struct sockaddr_in* addr)
{
	return uv_ip4_addr(ip,port,addr);
}
int UvSocket::inetString(const struct sockaddr_in* src, char* dst, size_t size)
{
	return uv_ip4_name(src,dst,size);
}


int  UvSocket::asyncAccept()
{
	return 0;
}
int  UvSocket::asyncConnect(string ip,int port)
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

int   UvSocket::nodelay(bool enable)
{
	return 0;
}
int  UvSocket::keepAlive(bool enable, unsigned int value)
{
	return 0;
}
int  UvSocket::simultaneousAccept(bool enable)
{
	return 0;
}
int   UvSocket::getPeername(struct sockaddr_in* name) 
{
	return 0;
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
			LOG_ERROR("create loop error\r\n");
			ret = -1;
			break;
		}

		struct sockaddr_in addr;

		ret = uv_ip4_addr(szAddr.c_str(),port,&addr);
		ASSERT(ret == 0);
		if (ret){
			LOG_ERROR("get ip error\r\n");
			break;
		}

		uv_tcp_t* tcp_stream = (uv_tcp_t*)malloc(sizeof uv_tcp_t);
		ret = uv_tcp_init(uv_loop, tcp_stream);
		ASSERT(ret == 0);
		if(ret){
			LOG_ERROR("create server socket error\r\n");
			break;
		}

		ret = uv_tcp_bind(tcp_stream,(const sockaddr*)&addr,0);
		if (ret)
		{
			LOG_ERROR("bind error \r\n");
			break;
		}

		UvTcpSocket* tcp = new UvTcpSocket(env, tcp_stream);
		return tcp;
	}while(0);

	return NULL;
}

UvTcpSocket::UvTcpSocket(LibuvUsageEnvironment* env,uv_tcp_t* uv_tcp)
	:UvSocket(env),m_uv_tcp(uv_tcp),m_uv_connect(NULL),m_uv_write(NULL)
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




int UvTcpSocket::asyncAccept()
{
	int ret = 0;
	do 
	{
		m_uv_tcp->data = this;
		ret = uv_listen((uv_stream_t*)m_uv_tcp,5,UvTcpSocket::on_connection);
		if (ret)
		{
			LOG_ERROR("listen error\r\n");
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
	int conditon = SOCKET_READABLE;
	do 
	{	
		m_newConnection = NULL;
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
			LOG_ERROR("tcp error \r\n");
			break;
		}

		ret = uv_accept(stream,(uv_stream_t*)client);
		ASSERT(ret == 0);
		if(ret)
		{
			LOG_ERROR("accept error\r\n");
			break;
		}
		struct sockaddr_in addrIn ;
		int addrSize = sizeof(sockaddr);
		uv_tcp_getsockname((uv_tcp_t*)client, (sockaddr*)&addrIn, &addrSize);
		struct sockaddr_in addrInPeer ;
		uv_tcp_getpeername((uv_tcp_t*)client, (sockaddr*)&addrInPeer, &addrSize);

		char serverIp[128],client[128];
		uv_ip4_name(&addrInPeer,client,128);
		uv_ip4_name(&addrIn,serverIp,128);

		LOG_DEBUG("%s:%d accept connection from %s:%d",serverIp,
			htons(addrIn.sin_port),client,htons(addrInPeer.sin_port));

		m_newConnection = new UvTcpSocket(m_env,(uv_tcp_t*)client);

		if(m_ioHandlerProc)
			m_ioHandlerProc(m_ClientData, conditon);

		return ;

	} while (0);

	conditon |= SOCKET_EXCEPTION;
	if(m_ioHandlerProc)
		m_ioHandlerProc(m_ClientData, conditon);
	
	if(client)
		free(client);
}


int  UvTcpSocket::asyncConnect(string ip,int port)
{
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
	int condition = SOCKET_WRITABLE;
	if(status != 0)
	condition |= SOCKET_EXCEPTION;
	if(m_ioHandlerProc)
	{
		m_ioHandlerProc(m_ClientData,condition);
	}
}




void UvTcpSocket::on_read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
	UvTcpSocket* pThis = reinterpret_cast<UvTcpSocket*> (stream->data);
	pThis->on_read_cb1(stream,nread,buf);
}
void UvTcpSocket::on_read_cb1(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
	m_readSize = nread;
	if(nread == 0) //nread might be 0, which does not indicate an error or EOF. This is equivalent to EAGAIN or EWOULDBLOCK under read
		return ;
	int condition = SOCKET_READABLE;
	if(nread < 0)
		condition |= SOCKET_EXCEPTION;
	if(m_ioHandlerProc)
		m_ioHandlerProc(m_ClientData, condition);
}






int	 UvTcpSocket::asyncReadStart(void* buffer, int size)
{
	m_buffer = buffer;
	m_bufferSize = size;

	m_uv_tcp->data = this;
	return uv_read_start((uv_stream_t*)m_uv_tcp, UvTcpSocket::on_alloc_recv, UvTcpSocket::on_read_cb);
}



int  UvTcpSocket::asyncWrite(const uv_buf_t data[], int count, const struct sockaddr* addr /*= NULL*/)
{
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

	int condition = SOCKET_WRITABLE;

	if (status !=0)
		condition |= SOCKET_EXCEPTION;

	if(m_ioHandlerProc)
		m_ioHandlerProc(m_ClientData, condition);
}



int  UvTcpSocket::getSockname(struct sockaddr_in* name)
{
	int size = sizeof sockaddr_in;
	return uv_tcp_getsockname(m_uv_tcp, (sockaddr*)name, &size);
}
int  UvTcpSocket::getPeername(struct sockaddr_in* name)
{
	int size = sizeof sockaddr_in;
	return uv_tcp_getpeername(m_uv_tcp, (sockaddr*)name, &size);
}

int  UvTcpSocket::nodelay(bool enable)
{
	return uv_tcp_nodelay(m_uv_tcp, enable?1:0);
}
int  UvTcpSocket::keepAlive(bool enable, unsigned int value)
{
	return uv_tcp_keepalive(m_uv_tcp,enable?1:0,value);
}
int  UvTcpSocket::simultaneousAccept(bool enable)
{
	return uv_tcp_simultaneous_accepts(m_uv_tcp,enable?1:0);
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
			LOG_ERROR("create loop error\r\n");
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

		UvUdpSocket* udp = new UvUdpSocket(env,uv_udp);
		return udp;

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


int UvUdpSocket::asyncReadStart(void* buffer, int size)
{

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
	if(nread == 0)
		return ;
	if(addr)
		m_lastSockaddr = *((sockaddr_in*)addr);
	m_readSize = nread; // The receive callback will be called with nread == 0 and addr == NULL when there is nothing to read, and with nread == 0 and addr != NULL when an empty UDP packet is received.
	m_flags = flags;
	int condition = SOCKET_READABLE;
	if(nread < 0)
		condition |= SOCKET_EXCEPTION;
	if(m_ioHandlerProc)
		m_ioHandlerProc(m_ClientData, condition);
}

int UvUdpSocket::asyncWrite(const uv_buf_t data[], int count,  const struct sockaddr* addr )
{
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

	int condition = SOCKET_WRITABLE;
	if(status != 0)
		condition |= SOCKET_EXCEPTION;
	if(m_ioHandlerProc)
		m_ioHandlerProc(m_ClientData, condition);
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

 int  UvUdpSocket::getSockname(struct sockaddr_in* name)
 {
	 int size = sizeof sockaddr_in;
	 return uv_udp_getsockname(m_uv_udp, (sockaddr*)name, &size);
 }

 int  UvUdpSocket::getPeername(struct sockaddr_in* name)
 {
	*name = m_lastSockaddr;
	return 0;
 }