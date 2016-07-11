#ifndef __RtmpClientSession_H
#define __RtmpClientSession_H

#include "UvServer.h"
#include "CClientSession.h"

class CRtmpClientSession : public CClientSession
{
	friend class UvConnection;
public:
	static CRtmpClientSession* createCRtmpClientSession(UvConnection* client);	
	~CRtmpClientSession(void);

	virtual int onTranscation();

protected:
	CRtmpClientSession(void);

protected:
	UvConnection* m_client;

};




#endif
