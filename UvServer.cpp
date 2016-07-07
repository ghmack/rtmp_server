#include "UvServer.h"


CUvServer* CUvServer::createUvServer(LibuvUsageEnvironment* env, string ip,int port)
{
	CUvServer* uvServer = NULL;
	do 
	{
		UvSocket* sock = UvTcpSocket::createUvTcpSokcet(env,ip,port);
		if (!sock)
		{
			break;
		}
		uvServer = new CUvServer(env,sock,ip,port);
		sock->assignBackgroundHandling(CUvServer::onConnection, uvServer);
		if (sock->asyncAccept() != 0 )
		{
			break;
		}

		LOG_INFO("create server success, ip %, port %d", ip.c_str(),port);

		return uvServer;
	} while (0);
	if (uvServer)
	{
		delete uvServer;
	}
	LOG_ERROR("create server error");
	return NULL;
}

CUvServer::CUvServer(){

};

CUvServer::CUvServer(LibuvUsageEnvironment* env, UvSocket* sockAccept,string ip, int port):
m_env(env),m_sockAccept(sockAccept),m_bindIp(ip),m_port(port)
{

}

CUvServer::~CUvServer()
{
	if (m_sockAccept)
	{
		delete m_sockAccept;
		m_sockAccept = NULL;
	}
};


void CUvServer::onConnection(void* clientData, int mask)
{
	if (mask & SOCKET_EXCEPTION)
	{
		LOG_ERROR("accept client error");
		return ;
	}
	CUvServer* pThis = reinterpret_cast<CUvServer*>(clientData);
	ASSERT(pThis);
	pThis->onConnection1();
}


void CUvServer::onConnection1()
{
	do 
	{
		UvSocket* conn = m_sockAccept->newConnection();
		ASSERT(conn);

		UvConnection* uvConn = UvConnection::createUvConnection(m_env,conn,this);
		if (uvConn)
		{
			m_clients.insert(uvConn);
		}

	} while (0);
}


void CUvServer::closeClient(UvConnection* uvConn)
{
	delete uvConn;
	uvConn = NULL;
}



//////////////////////////////////////////////////////////////////////////
UvConnection* 
UvConnection::createUvConnection(LibuvUsageEnvironment* env,UvSocket* client,CUvServer* server,int bufferSize)
{
	UvConnection* uvConn = new UvConnection(env,client,server,bufferSize);
	client->assignBackgroundHandling(UvConnection::onIo,uvConn);
	int ret = client->asyncReadStart(uvConn->m_Buffer,uvConn->m_bufferSize);
	if (ret != 0)
	{
		delete uvConn;
		return NULL;
	}
	return uvConn;
}

UvConnection::UvConnection(LibuvUsageEnvironment* env,UvSocket* client,CUvServer* server,int bufferSize)
	:m_env(env),m_sockConn(client),m_uvServer(server),m_bufferSize(bufferSize)
{
	m_Buffer = new char[bufferSize];
}
UvConnection::UvConnection()
{

}

UvConnection::~UvConnection()
{
	if (m_Buffer)
	{
		delete m_Buffer;
		m_Buffer = NULL;
	}
	
	if (m_sockConn)
	{
		delete m_sockConn;
		m_sockConn = NULL;
	}

	m_uvServer->m_clients.erase(this);
}

void UvConnection::onIo(void* param, int mask)
{
	UvConnection* pThis = reinterpret_cast<UvConnection*> (param);
	ASSERT(pThis);
	do 
	{
		if(mask & SOCKET_EXCEPTION)
		{
			if(mask & SOCKET_READABLE)
				LOG_WARN("read io error")
			else if(mask & SOCKET_WRITABLE)
			    LOG_WARN("write io error")

				break;
		}

		if(mask & SOCKET_READABLE)
		pThis->onIoRead();
		else if (mask & SOCKET_WRITABLE)
		pThis->onIoWrite();
		
		
		return ;

	} while (0);

	pThis->m_uvServer->closeClient(pThis);
	return ;
}

void UvConnection::onIoRead()
{

}

void UvConnection::onIoWrite()
{

}



