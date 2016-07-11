#ifndef _UVSERVER_H
#define _UVSERVER_H

#include "uv-env.h"
#include <string>
#include <set>
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

class SrsBuffer2
{
public:
	SrsBuffer2();
	virtual ~SrsBuffer2();
    /**
    * get the length of buffer. empty if zero.
    * @remark assert length() is not negative.
    */
    virtual int length();
    /**
    * get the buffer bytes.
    * @return the bytes, NULL if empty.
    */
    virtual char* bytes();
    /**
    * erase size of bytes from begin.
    * @param size to erase size of bytes. 
    *       clear if size greater than or equals to length()
    * @remark ignore size is not positive.
    */
    virtual void erase(int size);
    /**
    * append specified bytes to buffer.
    * @param size the size of bytes
    * @remark assert size is positive.
    */
    virtual void append(const char* bytes, int size);
private:
	string m_data;

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