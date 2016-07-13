#ifndef __RTMPDECODE_H
#define __RTMPDECODE_H

#include <string>
#include <map>
using namespace std;

#include "srs_kernel_error.hpp"
#include "srs_kernel_utility.hpp"
#include "srs_kernel_buffer.hpp"
#include "srs_kernel_stream.hpp"
#include "srs_kernel_log.hpp"
#include "srs_protocol_utility.hpp"
#include "srs_protocol_handshake.hpp"
#include "srs_protocol_stack.hpp"
#include "rtmp_const.h"

using namespace _srs_internal;



class CRtmpDecode
{
public:
	CRtmpDecode(void);
	virtual ~CRtmpDecode(void);
};

class CRtmpHandshake
{ 
public:
	typedef int (*handshakeResponse)(void* data, int size, void* param);
	typedef void (*onHandshakeSuccess)(void* param);
	CRtmpHandshake();
	virtual ~CRtmpHandshake();
	enum eum_state_hs
	{
		hs_state_init = 0,
		hs_state_c0c1 ,
		hs_state_c2,
		hs_state_successed,
	};
	typedef eum_state_hs eum_state_hs;
	void  handshakeWithClient(char* decodeBuffer, int size,
		handshakeResponse writeData,void* param,
		onHandshakeSuccess onSuccess, void* param2);
	int   create_s0s1s2(const char* c1, const char* c0c1,char* s0s1s2);
	eum_state_hs state();
private:
	string m_c0c1;
	string m_c2;
	eum_state_hs m_state;
};


class RtmpProtocolstack
{
public:
	RtmpProtocolstack(SrsBuffer2* decodeBuffer);
	virtual ~RtmpProtocolstack();

public:
	class AckWindowSize
	{
	public:
		int ack_window_size;
		int64_t acked_size;

		AckWindowSize():ack_window_size(0),acked_size(0){

		}
	};

	enum rtmp_decode_state 
	{
		decode_init = 0,
		decode_bh, //½âÂëbasic chunk header £¬1-3 bytes
		decode_mh, //½âÂëmessage header 0£¬3,7£¬11 bytes
		decode_ext_time, // 4bytes if exist
		decode_payload,		
		decede_completed
	};
	typedef rtmp_decode_state rtmp_decode_state;

protected:
	int  readBasicChunkHeader();
	int  readMsgHeader();
	int  readMsgPayload();
	int  onInnerRecvMessage(SrsMessage* msg);
	int  decode_message(SrsMessage* msg, SrsPacket** ppacket);
	int  do_decode_message(SrsMessageHeader& header, SrsStream* stream, SrsPacket** ppacket);

private:
	int _current_cid;
	map<int,SrsChunkStream*> chunk_streams;
	rtmp_decode_state _decode_state;
	int in_chunk_size;
	int out_chunk_size; 
	bool _wait_buffer; //need more bytes to decode, invoke io read to buffer
	AckWindowSize in_ack_size;
	char out_header_cache[SRS_CONSTS_RTMP_MAX_FMT0_HEADER_SIZE];
	std::map<double, std::string> requests;
	SrsBuffer2* in_buffer;
};





#endif