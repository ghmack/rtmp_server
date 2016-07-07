#ifndef _UVSERVER_H
#define _UVSERVER_H

#include "uv-env.h"
#include <string>
#include <set>
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
public:
	static UvConnection* createUvConnection(LibuvUsageEnvironment* env,UvSocket* client,CUvServer* server,int bufferSize= 4096);
	virtual ~UvConnection();
	
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
	void*	m_Buffer;
	int		m_bufferSize;
};



#endif