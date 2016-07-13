#ifndef _UVSERVER_H
#define _UVSERVER_H

#include "uv-env.h"
#include <string>
#include <set>
#include "srs_kernel_buffer.hpp"
#include "CClientSession.h"
using namespace std;


class UvConnection;
class CUvServer
{
public:
	static CUvServer* createUvServer(LibuvUsageEnvironment* env,string ip, int port);
	virtual ~CUvServer(void);
	void	closeClient(UvConnection* uvConn);
protected:
	CUvServer(LibuvUsageEnvironment* env, UvSocket* scokAccept,string ip, int port);
	CUvServer();
	static void onConnection(void* clientData, int mask);
	void onConnection1();


private:
	LibuvUsageEnvironment* m_env;
	UvSocket* m_sockAccept;
	string m_bindIp;
	int m_port;
	set<UvConnection*> m_clients;

	friend class UvConnection;


};




class UvConnection
{
	friend class CUvServer;
	friend class CRtmpClientSession;
public:
	static UvConnection* createUvConnection(LibuvUsageEnvironment* env,UvSocket* client,CUvServer* server,int bufferSize= 4096);
	virtual ~UvConnection();

	void close();
	
protected:
	UvConnection(LibuvUsageEnvironment* env,UvSocket* client,CUvServer* server,int bufferSize);
	UvConnection();

	static void onIo(void* param, int mask);
	void onIoRead();
	void onIoWrite();

private:
	CUvServer* m_uvServer;
	UvSocket*  m_sockConn;
	LibuvUsageEnvironment* m_env;
	char*	m_Buffer;
	int		m_bufferSize;
	CClientSession* m_clientSession;
	SrsBuffer2* m_decodeBuffer;
};



#endif