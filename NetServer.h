#pragma once


#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <string>
#include <set>

#include "srs_kernel_utility.hpp"
#include "srs_kernel_buffer.hpp"
#include "srs_protocol_stack.hpp"

using namespace std;

#define boost_error boost::system::error_code 

#define info_trace printf

#define IO_READ_BUFFER_SIZE 4096

inline void ThrowException(const char* exp,...) 
{ 
	char buffer[1000];
	//sprintf(buffer,"%s,File: %s, Line: %s \r\n",exp,__FILE__,__LINE__);
	//printf(buffer);
	va_list args;
	va_start(args,exp);
	vsprintf(buffer,exp,args);
	va_end(args);
	printf(buffer);
	throw buffer;
}

#define ThrExp(s,...) ThrowException(s,##__VA_ARGS__) 
#define ThrExpErr(s) ThrExp(s,__FILE__,__LINE__)

#define RTMP_MSG_SetChunkSize                   0x01
#define RTMP_MSG_AbortMessage                   0x02
#define RTMP_MSG_Acknowledgement                0x03
#define RTMP_MSG_UserControlMessage             0x04
#define RTMP_MSG_WindowAcknowledgementSize      0x05
#define RTMP_MSG_SetPeerBandwidth               0x06
#define RTMP_MSG_EdgeAndOriginServerCommand     0x07

#define RTMP_MSG_AMF3CommandMessage             17 // 0x11
#define RTMP_MSG_AMF0CommandMessage             20 // 0x14

#define RTMP_MSG_AMF0DataMessage                18 // 0x12
#define RTMP_MSG_AMF3DataMessage                15 // 0x0F

#define RTMP_MSG_AMF3SharedObject               16 // 0x10
#define RTMP_MSG_AMF0SharedObject               19 // 0x13

#define RTMP_MSG_AudioMessage                   8 // 0x08

#define RTMP_MSG_VideoMessage                   9 // 0x09

#define RTMP_MSG_AggregateMessage               22 // 0x16

#define RTMP_FMT_TYPE0                          0

#define RTMP_FMT_TYPE1                          1

#define RTMP_FMT_TYPE2                          2

#define RTMP_FMT_TYPE3                          3

#define RTMP_EXTENDED_TIMESTAMP                 0xFFFFFF

#define RTMP_AMF0_COMMAND_CONNECT               "connect"
#define RTMP_AMF0_COMMAND_CREATE_STREAM         "createStream"
#define RTMP_AMF0_COMMAND_CLOSE_STREAM          "closeStream"
#define RTMP_AMF0_COMMAND_PLAY                  "play"
#define RTMP_AMF0_COMMAND_PAUSE                 "pause"
#define RTMP_AMF0_COMMAND_ON_BW_DONE            "onBWDone"
#define RTMP_AMF0_COMMAND_ON_STATUS             "onStatus"
#define RTMP_AMF0_COMMAND_RESULT                "_result"
#define RTMP_AMF0_COMMAND_ERROR                 "_error"
#define RTMP_AMF0_COMMAND_RELEASE_STREAM        "releaseStream"
#define RTMP_AMF0_COMMAND_FC_PUBLISH            "FCPublish"
#define RTMP_AMF0_COMMAND_UNPUBLISH             "FCUnpublish"
#define RTMP_AMF0_COMMAND_PUBLISH               "publish"
#define RTMP_AMF0_DATA_SAMPLE_ACCESS            "|RtmpSampleAccess"
#define RTMP_AMF0_DATA_SET_DATAFRAME            "@setDataFrame"
#define RTMP_AMF0_DATA_ON_METADATA              "onMetaData"

/**
* band width check method name, which will be invoked by client.
* band width check mothods use SrsBandwidthPacket as its internal packet type,
* so ensure you set command name when you use it.
*/
// server play control
#define SRS_BW_CHECK_START_PLAY                 "onSrsBandCheckStartPlayBytes"
#define SRS_BW_CHECK_STARTING_PLAY              "onSrsBandCheckStartingPlayBytes"
#define SRS_BW_CHECK_STOP_PLAY                  "onSrsBandCheckStopPlayBytes"
#define SRS_BW_CHECK_STOPPED_PLAY               "onSrsBandCheckStoppedPlayBytes"

// server publish control
#define SRS_BW_CHECK_START_PUBLISH              "onSrsBandCheckStartPublishBytes"
#define SRS_BW_CHECK_STARTING_PUBLISH           "onSrsBandCheckStartingPublishBytes"
#define SRS_BW_CHECK_STOP_PUBLISH               "onSrsBandCheckStopPublishBytes"
// @remark, flash never send out this packet, for its queue is full.
#define SRS_BW_CHECK_STOPPED_PUBLISH            "onSrsBandCheckStoppedPublishBytes"

// EOF control.
// the report packet when check finished.
#define SRS_BW_CHECK_FINISHED                   "onSrsBandCheckFinished"
// @remark, flash never send out this packet, for its queue is full.
#define SRS_BW_CHECK_FINAL                      "finalClientPacket"

// data packets
#define SRS_BW_CHECK_PLAYING                    "onSrsBandCheckPlaying"
#define SRS_BW_CHECK_PUBLISHING                 "onSrsBandCheckPublishing"

/****************************************************************************
*****************************************************************************
****************************************************************************/
/**
* the chunk stream id used for some under-layer message,
* for example, the PC(protocol control) message.
*/
#define RTMP_CID_ProtocolControl                0x02
/**
* the AMF0/AMF3 command message, invoke method and return the result, over NetConnection.
* generally use 0x03.
*/
#define RTMP_CID_OverConnection                 0x03
/**
* the AMF0/AMF3 command message, invoke method and return the result, over NetConnection, 
* the midst state(we guess).
* rarely used, e.g. onStatus(NetStream.Play.Reset).
*/
#define RTMP_CID_OverConnection2                0x04
/**
* the stream message(amf0/amf3), over NetStream.
* generally use 0x05.
*/
#define RTMP_CID_OverStream                     0x05
/**
* the stream message(amf0/amf3), over NetStream, the midst state(we guess).
* rarely used, e.g. play("mp4:mystram.f4v")
*/
#define RTMP_CID_OverStream2                    0x08
/**
* the stream message(video), over NetStream
* generally use 0x06.
*/
#define RTMP_CID_Video                          0x06
/**
* the stream message(audio), over NetStream.
* generally use 0x07.
*/
#define RTMP_CID_Audio                          0x07



class RtmpConnection;
class RtmpNetServer;
class ConnectionMgr;
class CRtmpProtocol;
class CReadWriteIO;



class CReadWriteIO
{
public:
	CReadWriteIO(boost::asio::ip::tcp::socket& socket);
	~CReadWriteIO();

	void async_read(void* buffer, int size, boost::function<void (int,bool)> funBack);

	void async_write(void* buffer,int size,boost::function<void (int,bool)> funBack);

	uint64_t total_recv();

	uint64_t total_send();
protected:
	void onIO(int size, boost::system::error_code err,boost::function<void (int,bool)> funBack,bool bReadOpt);

private:
	boost::asio::ip::tcp::socket& _socket;
	uint64_t _recvSize;
	uint64_t _sendSize;

};





class IRtmpListener
{
public:
	virtual void onRecvMessage() = 0;
};

class CRtmpProtocolStack
{
public:
	class AckWindowSize
	{
	public:
		int _ack_window_size;
		int64_t _acked_size;

		AckWindowSize():_ack_window_size(0),_acked_size(0){
		}
	};
	CRtmpProtocolStack(CReadWriteIO* io);
	~CRtmpProtocolStack();

	void addListener(IRtmpListener* listener);
	void recvMessage(int size, bool err);
	void open();
	void onInnerRecvMessage(SrsMessage* msg);
	virtual int decode_message(SrsMessage* msg, SrsPacket** ppacket);

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
private:
	void readBasicChunkHeader();
	void readMsgHeader();
	void readMsgPayload();

	void responseAckMsg();

private:
	CReadWriteIO* _io;
	IRtmpListener* _listener;
	char _buffer[IO_READ_BUFFER_SIZE];
	SrsBuffer* _inBuffer;
	int _current_cid;
	map<int,SrsChunkStream*> _mapChunkStream;
	rtmp_decode_state _decode_state;
	int _in_chunk_size;
	int _out_chunk_size; 
	bool _wait_buffer; //need more bytes to decode, invoke io read to buffer
	AckWindowSize _in_ack_size;
	char _out_header_cache[SRS_CONSTS_RTMP_MAX_FMT0_HEADER_SIZE];
};



class CRtmpHandeShake
{
public:
	CRtmpHandeShake(CReadWriteIO* io,boost::function<void ()> handshakedFunc);
	~CRtmpHandeShake();

	 enum eum_state_hs
	{
		hs_state_init = 0,
		hs_state_c0c1 ,
		hs_state_c2,
		hs_state_successed,
	};
	typedef eum_state_hs eum_state_hs;
	void handShakeWithClient();
	void handleClient(int size, bool bErr);
	eum_state_hs state();
private:
	CReadWriteIO* _io;
	char* _buffer[IO_READ_BUFFER_SIZE];
	string _c0c1;
	string _c2;
	eum_state_hs _hs_state;
	string _sendBuffer;
	boost::function<void ()> _onHandshaked;
	
};




class CRtmpComplexHandShake
{
public:
	CRtmpComplexHandShake(CReadWriteIO* io);
	~CRtmpComplexHandShake();
	enum eumHandShakeState{

	};

	void handShakeWithClient();
	void handShakeWithServer();


private:
	CReadWriteIO* _io;

};

class CRtmpSimpleHandShake
{
public:
	CRtmpSimpleHandShake();
	~CRtmpSimpleHandShake();

	void handShakeWithClient();
	void handShakeWithServer();

private:
	CReadWriteIO* _io;
};


class RtmpConnection : public enable_shared_from_this<RtmpConnection>
{
public:
	RtmpConnection(boost::asio::io_service& ios);
	~RtmpConnection();

	boost::asio::ip::tcp::socket& socketRef();
	void close();
	void start();
private:
	boost::asio::ip::tcp::socket _socket;
	CReadWriteIO _io;
	//CRtmpProtocolStack _rtmpStack;

};

typedef shared_ptr<RtmpConnection> RtmpConnection_ptr; 


class ConnectionMgr
{
public:
	ConnectionMgr();
	~ConnectionMgr();
	void add(RtmpConnection_ptr conPtr);
private:

	set<RtmpConnection_ptr> _conSet;
};

class RtmpNetServer
{
public:
	RtmpNetServer(string ip,int port);
	~RtmpNetServer();

	void start();
	void handle_accept(const boost::system::error_code& err);

private:
	boost::asio::io_service _ios;
	boost::asio::ip::tcp::acceptor _acceptor;
	string _ip;
	int _port;
	RtmpConnection_ptr _rtmpConPtr;
	ConnectionMgr _mgr;

};

