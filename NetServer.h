#pragma once


#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <string>
#include <map>
#include <set>

#include "srs_core.hpp"
#include "srs_core_autofree.hpp"
#include "srs_kernel_utility.hpp"
#include "srs_kernel_stream.hpp"
#include "srs_protocol_amf0.hpp"
#include "srs_kernel_buffer.hpp"
#include "srs_protocol_stack.hpp"
#include "srs_protocol_rtmp.hpp"
#include "srs_protocol_utility.hpp"
#include "srs_kernel_log.hpp"
using namespace std;

#include "rtmp_const.h"

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












class RtmpConnection;
class RtmpNetServer;
class ConnectionMgr;
class CRtmpProtocol;
class CReadWriteIO;
class CRtmpHandeShake;


class CReadWriteIO
{
public:
	CReadWriteIO(boost::asio::ip::tcp::socket& socket);
	~CReadWriteIO();

	void async_read(void* buffer, int size, boost::function<void (int,bool)> funBack);

	void async_write(void* buffer,int size,boost::function<void (int,bool)> funBack);

	int writev(const iovec *iov, int iov_size, ssize_t* nwrite);

	int write(void* buf, size_t size, ssize_t* nwrite);

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
		int ack_window_size;
		int64_t acked_size;

		AckWindowSize():ack_window_size(0),acked_size(0){

		}
	};
	CRtmpProtocolStack(CReadWriteIO* io);
	~CRtmpProtocolStack();

	void addListener(IRtmpListener* listener);
	void recvMessage(int size, bool err);
	void open(string data);
	int  onInnerRecvMessage(SrsMessage* msg);
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
public:
	void readBasicChunkHeader();
	void readMsgHeader();
	void readMsgPayload();

	//void read_basic_chunk_header();
	//void read_packet_header();
	//void read_packet_payload();

	void responseAckMsg();
	void sendPacket(SrsPacket* packet, int stream_id);
	//void response_ack_msg();
	//void send_packet(SrsPacket* packet, int stream_id);

	int do_decode_message(SrsMessageHeader& header, SrsStream* stream, SrsPacket** ppacket);
	int on_send_packet(SrsMessage* msg, SrsPacket* packet);
	int do_send_message(SrsMessage* msg, SrsPacket* packet);

	int response_ping_message(int32_t timestamp);
	int send_and_free_packet(SrsPacket* packet, int stream_id);
	int send_and_free_message(SrsMessage* msg, int stream_id);


	virtual int identify_create_stream_client(SrsCreateStreamPacket* req, int stream_id, SrsRtmpConnType& type, std::string& stream_name, double& duration);
	virtual int identify_fmle_publish_client(SrsFMLEStartPacket* req, SrsRtmpConnType& type, std::string& stream_name);
	virtual int identify_flash_publish_client(SrsPublishPacket* req, SrsRtmpConnType& type, std::string& stream_name);
	virtual int identify_play_client(SrsPlayPacket* req, SrsRtmpConnType& type, std::string& stream_name, double& duration);
private:
	int onSetChunkSize(SrsPacket* packet);
	int onAckWindowSize(SrsPacket* packet);
	int onUserControl(SrsPacket* packet);

	int onConnection(SrsPacket* packet);
	int onCreateStream(SrsCreateStreamPacket* packet);

private:
	int set_window_ack_size(int ack_size);
	int set_peer_bandwidth(int bandwidth, int type);
	int response_connect_app(SrsRequest *req, const char* server_ip);
	int set_chunk_size(int chunk_size);

	int start_play(int stream_id);
	int start_flash_publish(int stream_id);
private:

	//protocol layer
	CReadWriteIO* _io;
	IRtmpListener* _listener;
	char _buffer[IO_READ_BUFFER_SIZE];
	SrsBuffer* in_buffer;
	int _current_cid;
	map<int,SrsChunkStream*> chunk_streams;
	rtmp_decode_state _decode_state;
	int in_chunk_size;
	int out_chunk_size; 
	bool _wait_buffer; //need more bytes to decode, invoke io read to buffer
	AckWindowSize in_ack_size;
	char out_header_cache[SRS_CONSTS_RTMP_MAX_FMT0_HEADER_SIZE];
	std::map<double, std::string> requests;

	//application layer
	SrsRequest* req;
	SrsResponse* res;
	//SrsStSocket* skt;
	//SrsRtmpServer* rtmp;
	//SrsRefer* refer;
	//SrsBandwidth* bandwidth;
	// elapse duration in ms
	// for live play duration, for instance, rtmpdump to record.
	// @see https://github.com/winlinvip/simple-rtmp-server/issues/47
	int64_t duration;
	//SrsKbps* kbps;
	SrsRtmpConnType rtmpConnType;

	CRtmpHandeShake* handshake;

	friend class RtmpConnection;
public:
	bool _hasSendAvcCfg;
	bool _hasSendAacCfg;

};



class CRtmpHandeShake
{
public:
	CRtmpHandeShake(CReadWriteIO* io,boost::function<void (string )> handshakedFunc);
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
	int create_s0s1s2(const char* c1, const char* c0c1,char* s0s1s2);
	eum_state_hs state();
private:
	CReadWriteIO* _io;
	char* _buffer[IO_READ_BUFFER_SIZE];
	string _c0c1;
	string _c2;
	eum_state_hs _hs_state;
	string _sendBuffer;
	boost::function<void (string)> _onHandshaked;
	
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
	CRtmpProtocolStack* _rtmpProtocol;


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

