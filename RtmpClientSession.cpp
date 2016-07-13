#include "RtmpClientSession.h"


CRtmpClientSession::CRtmpClientSession(void)
{

}


CRtmpClientSession::~CRtmpClientSession(void)
{

}

CRtmpClientSession::CRtmpClientSession(UvConnection* client):m_client(client)
{

}

CRtmpClientSession* CRtmpClientSession::createCRtmpClientSession(UvConnection* client)
{
	return new CRtmpClientSession(client);
}

int CRtmpClientSession::onTranscation()
{
	do 
	{
		if (m_handshake.state() != CRtmpHandshake::hs_state_successed)
		{
			SrsBuffer2* buffer = m_client->m_decodeBuffer;
			m_handshake.handshakeWithClient(buffer->bytes(),buffer->size(),
				CRtmpClientSession::handshakeResponse,this,
				CRtmpClientSession::onHandshakeSuccess,this);
		}

		do 
		{


		} while (0);



	} while (0);


	return 0;
}


int CRtmpClientSession::handshakeResponse(void* data, int size,void* param)
{
	CRtmpClientSession* pThis = reinterpret_cast<CRtmpClientSession*>(param);
	ASSERT(pThis);
	uv_buf_t uvData;
	uvData.base = (char*)data;
	uvData.len = size;
	return pThis->m_client->m_sockConn->asyncWrite(&uvData,1,NULL);
}

void CRtmpClientSession::onHandshakeSuccess(void* param)
{
	CRtmpClientSession* pThis = reinterpret_cast<CRtmpClientSession*>(param);
	ASSERT(pThis);
	pThis->m_client->m_decodeBuffer->erase(3073);
	pThis->onTranscation();
}