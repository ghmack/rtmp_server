#ifndef __RtmpClientSession_H
#define __RtmpClientSession_H

#include "UvServer.h"
#include "CClientSession.h"
#include "RtmpDecode.h"



class CRtmpClientSession : public CClientSession
{
	friend class UvConnection;
public:
	static CRtmpClientSession* createCRtmpClientSession(UvConnection* client);	
	CRtmpClientSession(UvConnection* client);
	~CRtmpClientSession(void);

	virtual int onTranscation();

protected:
	CRtmpClientSession(void);
	static int handshakeResponse(void* data, int size,void* param);
	static void onHandshakeSuccess(void* param);

protected:
	UvConnection* m_client;
	CRtmpHandshake m_handshake;

};




#endif
