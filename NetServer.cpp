#include "NetServer.h"
#include <stdio.h>

#include <vector>




class deomoLiveSource
{
public:
	deomoLiveSource(string streamName = "")
	{
		avcConfigData = NULL;
		aacConfigData = NULL;
	}
	~deomoLiveSource()
	{

	}

	void addSubscriber(CRtmpProtocolStack* subscriber)
	{
		m_subscribers.push_back(subscriber);
	}
	void removeSubscriber(CRtmpProtocolStack* subscriber)
	{
		for (int i = 0; i < m_subscribers.size();i++)
		{
			CRtmpProtocolStack* item = m_subscribers.at(i);
			if (subscriber == item)
			{
				m_subscribers.erase(m_subscribers.begin() + i);
			}
		}
	}
	void on_publishData(SrsMessage* srsMessage,int streamId)
	{
		if (!avcConfigData && srsMessage->header.is_video() &&
			srsMessage->payload[0] == 0x17 &&
			srsMessage->payload[1] == 0x00 )
		{
			avcConfigData = new SrsCommonMessage();
			avcConfigData->header = srsMessage->header;
			avcConfigData->payload = new char[srsMessage->size];
			avcConfigData->size = srsMessage->size;
			memmove(avcConfigData->payload,srsMessage->payload,srsMessage->size);
		}
		if (!aacConfigData && srsMessage->header.is_audio() &&
			(srsMessage->payload[0] & 0xa0 == 0xa0) &&
			 srsMessage->payload[1] == 0x00
			)
		{
			aacConfigData = new SrsCommonMessage();
			aacConfigData->header = srsMessage->header;
			aacConfigData->payload = new char[srsMessage->size];
			aacConfigData->size = srsMessage->size;
			memmove(aacConfigData->payload,srsMessage->payload,srsMessage->size);
		}
		for (int i = 0; i < m_subscribers.size();i++)
		{
			CRtmpProtocolStack* subcriber = m_subscribers.at(i);
			if (subcriber)
			{
				if( !subcriber->_hasSendAvcCfg && avcConfigData && srsMessage->header.is_video())
				{
					subcriber->send_and_free_message(avcConfigData,streamId);
					subcriber->_hasSendAvcCfg = true;
				}
				else if(!subcriber->_hasSendAacCfg && aacConfigData && srsMessage->header.is_audio())
				{
					subcriber->send_and_free_message(aacConfigData,streamId);
					subcriber->_hasSendAacCfg = true;
				}
				else
				{
					subcriber->send_and_free_message(srsMessage,streamId);
				}
				
			}
		}

	}
private:
	vector<CRtmpProtocolStack*> m_subscribers;
	SrsCommonMessage* avcConfigData;
	SrsCommonMessage* aacConfigData;

};


deomoLiveSource* g_liveSource = new deomoLiveSource();



CReadWriteIO::CReadWriteIO(boost::asio::ip::tcp::socket& socket):_socket(socket)
{
	_sendSize = 0;
	_recvSize = 0;
}

CReadWriteIO::~CReadWriteIO()
{

}

void CReadWriteIO::async_read(void* buffer, int size, boost::function<void (int,bool)> readBack)
{
	_socket.async_read_some(boost::asio::mutable_buffers_1(buffer,size),
		boost::bind(&CReadWriteIO::onIO,this,boost::asio::placeholders::bytes_transferred,
		boost::asio::placeholders::error,readBack,true));
}

void CReadWriteIO::async_write(void* buffer,int size,boost::function<void (int,bool)> writeBack)
{
	boost::asio::async_write(_socket,boost::asio::const_buffers_1(buffer,size),
		boost::bind(&CReadWriteIO::onIO,this,boost::asio::placeholders::bytes_transferred,
		boost::asio::placeholders::error,writeBack,false));
}


int CReadWriteIO::writev(const iovec *iov, int iov_size, ssize_t* nwrite)
{
	string data;
	for (int i = 0 ;i< iov_size;i++)
	{
		data.append((char*)iov[i].iov_base,iov[i].iov_len);
	}
	if(data.size() > 0)
	async_write((void*)data.data(),data.size(),NULL);

	return 0;
}

int CReadWriteIO::write(void* buf, size_t size, ssize_t* nwrite)
{
	if (size > 0)
	{
		async_write(buf,size,NULL);
	}
	return 0;
}

uint64_t CReadWriteIO::total_recv()
{
	return _recvSize;
}

uint64_t CReadWriteIO::total_send()
{
	return _sendSize;
}

void CReadWriteIO::onIO(int size, boost::system::error_code err,boost::function<void (int,bool)> funBack,bool bReadOpt)
{
	try{
		if (err)
		{
			info_trace("%s io operate error\r\n",bReadOpt?"read":"write");
			_socket.close();
		}
		bReadOpt?_recvSize += size:_sendSize += size;
		if (funBack==NULL)
		{
			if(err)
				ThrExp(bReadOpt?"read io operate error\r\n":"write io operate error\r\n");
			return ;
		}
		funBack(size,!err);
	}catch(...)
	{
		return;
	}
}




RtmpNetServer::RtmpNetServer(string ip,int port):_ios(),_acceptor(_ios),_rtmpConPtr(new RtmpConnection(_ios))
{
	_ip = ip;
	_port = port;
}
RtmpNetServer::~RtmpNetServer()
{

}


void RtmpNetServer::start()
{
	boost::asio::ip::tcp::resolver reslver(_ios);
	char szPort[20];
	sprintf(szPort,"%d",_port);
	boost::asio::ip::tcp::resolver::query query(_ip,szPort);
	boost::asio::ip::tcp::endpoint endpoint = *reslver.resolve(query);
	_acceptor.open(endpoint.protocol());
	_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));

	_acceptor.bind(endpoint);
	_acceptor.listen();
	_acceptor.async_accept(_rtmpConPtr->socketRef(),boost::bind(&RtmpNetServer::handle_accept,
		this,boost::asio::placeholders::error));
	_ios.run();

}

void RtmpNetServer::handle_accept(const boost::system::error_code& err)
{
	if (!err)
	{
		
		_mgr.add(_rtmpConPtr);		
		_rtmpConPtr->start();
		_rtmpConPtr.reset(new RtmpConnection(_ios));
		_acceptor.async_accept(_rtmpConPtr->socketRef(),boost::bind(&RtmpNetServer::handle_accept,
			this,boost::asio::placeholders::error));
	}
	else
	{
		info_trace("accept socket error\r\n");
	}
}


//////////////////////////////////////////////////////////////////////////
ConnectionMgr::ConnectionMgr()
{

}
ConnectionMgr::~ConnectionMgr()
{

}

void ConnectionMgr::add(RtmpConnection_ptr conPtr)
{
	_conSet.insert(conPtr);
}



//////////////////////////////////////////////////////////////////////////
RtmpConnection::RtmpConnection(boost::asio::io_service& ios):_socket(ios),_io(_socket)
{
	_rtmpProtocol = new CRtmpProtocolStack(&_io);
}
RtmpConnection::~RtmpConnection()
{
	
}

boost::asio::ip::tcp::socket& RtmpConnection::socketRef()
{
	return _socket;
}

void RtmpConnection::close()
{
	_socket.close();
}

void RtmpConnection::start()
{
	_rtmpProtocol->handshake->handShakeWithClient();
}



//////////////////////////////////////////////////////////////////////////
CRtmpHandeShake::CRtmpHandeShake(CReadWriteIO* io,boost::function<void ()> func):_io(io),_onHandshaked(func)
{
	_hs_state = hs_state_init;
	
}

CRtmpHandeShake::~CRtmpHandeShake()
{

}

CRtmpHandeShake::eum_state_hs CRtmpHandeShake::state()
{
	return _hs_state;
}
void CRtmpHandeShake::handShakeWithClient()
{
	if(_hs_state != hs_state_successed)
		_io->async_read(_buffer,IO_READ_BUFFER_SIZE,boost::bind(&CRtmpHandeShake::handleClient,this,_1,_2));
}

void CRtmpHandeShake::handleClient(int size, bool bErr)
{
	if (_hs_state == hs_state_init)
	{
		_c0c1.append((char*)_buffer,size);
		if (_c0c1.size() == 1537)
		{
			_hs_state = hs_state_c2;
			char s0s1s2[3073] = {0};
			s0s1s2[0] = 0x03;
			uint32_t tm = time(NULL);
			memcpy(s0s1s2 + 1,&tm,4);
			memcpy(s0s1s2 + 5,_c0c1.data() + 1,4);
			memcpy(s0s1s2 + 1537,_c0c1.data(),1536);
			_sendBuffer.clear();
			_sendBuffer.append(s0s1s2,3073);
			_io->async_write((void*)_sendBuffer.data(),3073,NULL);
			_hs_state = hs_state_c2;
		}
	}
	else if (_hs_state == hs_state_c2)
	{
		_c2.append((char*)_buffer,size);
		if (_c2.size()==1536)
		{
			_hs_state = hs_state_successed;
			_onHandshaked();
			return ;
		}
	}
	handShakeWithClient();

}



//////////////////////////////////////////////////////////////////////////
CRtmpProtocolStack::CRtmpProtocolStack(CReadWriteIO* io):_io(io),_decode_state(decode_init)
{
	in_chunk_size = out_chunk_size = 128;
	_wait_buffer = false;
	_decode_state = decode_init;
	in_buffer = new SrsBuffer();

	//in_chunk_size = 4096; //do not set this beginning
	req = new SrsRequest();
	res = new SrsResponse();
	//skt = new SrsStSocket(client_stfd);
	//rtmp = new SrsRtmpServer(skt);
	//refer = new SrsRefer();
	//bandwidth = new SrsBandwidth();
	duration = 0;
	//kbps = new SrsKbps();
	//kbps->set_io(skt, skt);
	_hasSendAvcCfg = false;
	_hasSendAacCfg = false;
	handshake = new CRtmpHandeShake(_io, boost::bind(&CRtmpProtocolStack::open,this));
}

CRtmpProtocolStack::~CRtmpProtocolStack()
{

}

void CRtmpProtocolStack::open()
{
	_io->async_read(_buffer,IO_READ_BUFFER_SIZE,boost::bind(&CRtmpProtocolStack::recvMessage,this,_1,_2));
}

void CRtmpProtocolStack::readBasicChunkHeader()
{
	if (_decode_state == decode_init)
	{
		char fmt = 0;
		int cid = 0;
		int bh_size = 1;
		char* p = in_buffer->bytes();
		if (in_buffer->length() >= 1)
		{			
			fmt = (*p >> 6) & 0x03;
			cid = *p & 0x3f;
			if (cid > 1)
			{
				_decode_state = decode_mh;
			}
			else if (cid == 1)
			{
				if (in_buffer->length() >=2 )
				{
					cid = 64;
					cid += (u_int8_t)*(++p);
					bh_size = 2;
					_decode_state = decode_mh;
				}
				else 
				{	_wait_buffer = true; }
			}
			else if (cid == 0)
			{
				if (in_buffer->length() >=3)
				{
					cid = 64;
					cid += (u_int8_t)*(++p);
					cid += ((u_int8_t)*(++p)) * 256;
					bh_size = 3;
					_decode_state = decode_mh;
				}
				else 
				{	_wait_buffer = true; }
			}
			else
			{
				srs_assert(0);
			}
			srs_verbose("read basic header success. fmt=%d, cid=%d, bh_size=%d", fmt, cid, bh_size);

			if (_decode_state == decode_mh)
			{
				_current_cid = cid;
				in_buffer->erase(bh_size);
				if (chunk_streams.find(cid) == chunk_streams.end())
				{
					chunk_streams[cid] = new SrsChunkStream(cid);
					chunk_streams[cid]->fmt = fmt;
				}
				else
				{
					chunk_streams[cid]->fmt = fmt;
				}
			}
		}
		else //if (in_buffer->length() >= 1)
		{	
			_wait_buffer = true; 
		}
	}
}
void CRtmpProtocolStack::recvMessage(int size, bool err)
{
	if (!err)
	{
		//ThrExp("recv message error");
		srs_error("recv message error");
		g_liveSource->removeSubscriber(this);
		return ;
	}
	in_buffer->append(_buffer,size);

	while(true)
	{
		do 
		{
			readBasicChunkHeader();
			if(_wait_buffer) 
				break;
			readMsgHeader();
			if(_wait_buffer) 
				break;
			readMsgPayload();
			if(in_buffer->length() == 0)
				_wait_buffer = true;
		} while (0);
		
		if (_wait_buffer)
		{
			_io->async_read(_buffer,IO_READ_BUFFER_SIZE,boost::bind(&CRtmpProtocolStack::recvMessage,this,_1,_2));
			_wait_buffer = false;
			break;
		}
		
	}

}


void CRtmpProtocolStack::readMsgHeader()
{
	if (_decode_state == decode_mh)
	{
		if(chunk_streams.find(_current_cid)==chunk_streams.end())
			ThrExp("find cid %d error",_current_cid);
		SrsChunkStream* chunk = chunk_streams[_current_cid];
		char fmt = chunk->fmt;
		bool is_first_chunk_of_msg = !chunk->msg;

		if (chunk->msg_count == 0 && fmt != RTMP_FMT_TYPE0) {
			if (chunk->cid == RTMP_CID_ProtocolControl && fmt == RTMP_FMT_TYPE1) {
				srs_warn("accept cid=2, fmt=1 to make librtmp happy.");
			} else {
				// must be a RTMP protocol level error.
				int ret = ERROR_RTMP_CHUNK_START;
				srs_error("chunk stream is fresh, fmt must be %d, actual is %d. cid=%d, ret=%d", 
					RTMP_FMT_TYPE0, fmt, chunk->cid, ret);
				ThrExpErr("error");
			}
		}

		// when exists cache msg, means got an partial message,
		// the fmt must not be type0 which means new message.
		if (chunk->msg && fmt == RTMP_FMT_TYPE0) {
			int ret = ERROR_RTMP_CHUNK_START;
			srs_error("chunk stream exists, "
				"fmt must not be %d, actual is %d. ret=%d", RTMP_FMT_TYPE0, fmt, ret);
			ThrExpErr("chunk start error");
		}

		// create msg when new chunk stream start
		//if (!chunk->msg) {
		//	chunk->msg = new SrsCommonMessage();
		//	srs_verbose("create message for new chunk, fmt=%d, cid=%d", fmt, chunk->cid);
		//}

		static char mh_sizes[] = {11,7,3,0};
		char mh_size = mh_sizes[fmt];
		char* p = in_buffer->bytes();
		if (in_buffer->length() >= mh_size)
		{
			// create msg when new chunk stream start
			if (!chunk->msg) {
				chunk->msg = new SrsCommonMessage();
				srs_verbose("create message for new chunk, fmt=%d, cid=%d", fmt, chunk->cid);
			}
			if (fmt <= RTMP_FMT_TYPE2) 
			{
				char* pp = (char*)&chunk->header.timestamp_delta;
				pp[2] = *p++;
				pp[1] = *p++;
				pp[0] = *p++;
				pp[3] = 0;

				chunk->extended_timestamp = (chunk->header.timestamp_delta >= RTMP_EXTENDED_TIMESTAMP);
				if (!chunk->extended_timestamp) 
				{
					if (fmt == RTMP_FMT_TYPE0)
						chunk->header.timestamp = chunk->header.timestamp_delta;
					else
						chunk->header.timestamp += chunk->header.timestamp_delta;
				}

				if (fmt <= RTMP_FMT_TYPE1) 
				{
					int32_t payload_length = 0;
					pp = (char*)&payload_length;
					pp[2] = *p++;
					pp[1] = *p++;
					pp[0] = *p++;
					pp[3] = 0;

					if (!is_first_chunk_of_msg && chunk->header.payload_length != payload_length) 
					{
						int ret = ERROR_RTMP_PACKET_SIZE;
						srs_error("msg exists in chunk cache, "
							"size=%d cannot change to %d, ret=%d", 
							chunk->header.payload_length, payload_length, ret);
						ThrExpErr("rtmp msg header error");
					}

					chunk->header.payload_length = payload_length;
					chunk->header.message_type = *p++;
					if (fmt == RTMP_FMT_TYPE0) 
					{
						pp = (char*)&chunk->header.stream_id;
						pp[0] = *p++;
						pp[1] = *p++;
						pp[2] = *p++;
						pp[3] = *p++;
						srs_verbose("header read completed. fmt=%d, mh_size=%d, ext_time=%d, time=%"PRId64", payload=%d, type=%d, sid=%d", 
							fmt, mh_size, chunk->extended_timestamp, chunk->header.timestamp, chunk->header.payload_length, 
							chunk->header.message_type, chunk->header.stream_id);
					} 
				}
			}
			else
			{
				srs_assert(fmt==3);
				if (is_first_chunk_of_msg && !chunk->extended_timestamp) 
				{
					chunk->header.timestamp += chunk->header.timestamp_delta;
				}
			}

			if (chunk->extended_timestamp)
			{
				_decode_state = decode_ext_time;
			}
			else
			{
				_decode_state = decode_payload;
				chunk->header.timestamp &= 0x7fffffff;
				srs_assert(chunk->header.payload_length >= 0);
				chunk->msg->header = chunk->header;
				// increase the msg count, the chunk stream can accept fmt=1/2/3 message now.
				chunk->msg_count++;
			}
			in_buffer->erase(mh_size);
		}
		else //if (in_buffer->length() >= mh_size)
		{	_wait_buffer = true; }

	}

	if (_decode_state == decode_ext_time)
	{
		if (in_buffer->length() >= 4)
		{
			if(chunk_streams.find(_current_cid)==chunk_streams.end())
				ThrExp("find cid %d error",_current_cid);
			SrsChunkStream* chunk = chunk_streams[_current_cid];
			char fmt = chunk->fmt;
			bool is_first_chunk_of_msg = !chunk->msg;
			char* p = in_buffer->bytes();

			u_int32_t timestamp = 0x00;
			char* pp = (char*)&timestamp;
			pp[3] = *p++;
			pp[2] = *p++;
			pp[1] = *p++;
			pp[0] = *p++;

			timestamp &= 0x7fffffff;

			u_int32_t chunk_timestamp = chunk->header.timestamp;
			if (!is_first_chunk_of_msg && chunk_timestamp > 0 && chunk_timestamp != timestamp) {
				srs_info("no 4bytes extended timestamp in the continued chunk");
			} else {
				chunk->header.timestamp = timestamp;
			}

			_decode_state = decode_payload;
			in_buffer->erase(4);
			chunk->header.timestamp &= 0x7fffffff;
			srs_assert(chunk->header.payload_length >= 0);
			chunk->msg->header = chunk->header;
			// increase the msg count, the chunk stream can accept fmt=1/2/3 message now.
			chunk->msg_count++;
		}
		else 
		{	_wait_buffer = true; }
	}

	return ;
}


void CRtmpProtocolStack::readMsgPayload()
{
	if (_decode_state == decode_payload)
	{
		if(chunk_streams.find(_current_cid)==chunk_streams.end())
			ThrExp("find cid %d error",_current_cid);
		SrsChunkStream* chunk = chunk_streams[_current_cid];

		if (chunk->header.payload_length <= 0) { //empty message
			srs_trace("get an empty RTMP "
				"message(type=%d, size=%d, time=%"PRId64", sid=%d)", chunk->header.message_type, 
				chunk->header.payload_length, chunk->header.timestamp, chunk->header.stream_id);

			//onMessagePop(); //igore empty messages
			srs_freep(chunk->msg);
			chunk->msg = NULL;
			//chunk->msg_count =0;
			_decode_state == decode_init;
			return ;
		}
		srs_assert(chunk->header.payload_length > 0);

		int payload_size = chunk->header.payload_length - chunk->msg->size;
		payload_size = srs_min(payload_size, in_chunk_size);
		srs_verbose("chunk payload size is %d, message_size=%d, received_size=%d, in_chunk_size=%d", 
			payload_size, chunk->header.payload_length, chunk->msg->size, in_chunk_size);
		if (!chunk->msg->payload) {
			chunk->msg->payload = new char[chunk->header.payload_length];
			srs_verbose("create empty payload for RTMP message. size=%d", chunk->header.payload_length);
		}

		if (in_buffer->length() >= payload_size)
		{
			memcpy(chunk->msg->payload + chunk->msg->size, in_buffer->bytes(), payload_size);
			in_buffer->erase(payload_size);
			chunk->msg->size += payload_size;

			//srs_verbose("chunk payload read completed. bh_size=%d, mh_size=%d, payload_size=%d", bh_size, mh_size, payload_size);

			// got entire RTMP message?
			if (chunk->header.payload_length == chunk->msg->size) {							
				srs_verbose("get entire RTMP message(type=%d, size=%d, time=%"PRId64", sid=%d)", 
					chunk->header.message_type, chunk->header.payload_length, 
					chunk->header.timestamp, chunk->header.stream_id);
				onInnerRecvMessage(chunk->msg);
				srs_freep(chunk->msg);
				chunk->msg = NULL;
				//chunk->msg_count =0;
				_decode_state = decode_init;
				return ;
			}
			_decode_state = decode_init;
			srs_verbose("get partial RTMP message(type=%d, size=%d, time=%"PRId64", sid=%d), partial size=%d", 
				chunk->header.message_type, chunk->header.payload_length, 
				chunk->header.timestamp, chunk->header.stream_id,
				chunk->msg->size);
			
		}
		else 
		{	
			_wait_buffer = true;
		}

	}
	
}

int  CRtmpProtocolStack::onInnerRecvMessage(SrsMessage* msg)
{
	srs_assert(msg);

	if (msg->size <= 0 || msg->header.payload_length <= 0) {
		srs_trace("ignore empty message(type=%d, size=%d, time=%"PRId64", sid=%d).",
			msg->header.message_type, msg->header.payload_length,
			msg->header.timestamp, msg->header.stream_id);
		srs_freep(msg);
		return ERROR_SUCCESS;
	}
	SrsPacket* ppacket = NULL;
	decode_message(msg, &ppacket);




}


void CRtmpProtocolStack::responseAckMsg()
{

}

void CRtmpProtocolStack::sendPacket(SrsPacket* packet, int stream_id)
{

}

int CRtmpProtocolStack::decode_message(SrsMessage* msg, SrsPacket** ppacket)
{
	*ppacket = NULL;

	int ret = ERROR_SUCCESS;

	srs_assert(msg != NULL);
	srs_assert(msg->payload != NULL);
	srs_assert(msg->size > 0);

	SrsStream stream;

	// initialize the decode stream for all message,
	// it's ok for the initialize if fast and without memory copy.
	if ((ret = stream.initialize(msg->payload, msg->size)) != ERROR_SUCCESS) {
		srs_error("initialize stream failed. ret=%d", ret);
		return ret;
	}
	srs_verbose("decode stream initialized success");

	// decode the packet.
	SrsPacket* packet = NULL;
	if ((ret = do_decode_message(msg->header, &stream, &packet)) != ERROR_SUCCESS) {		
		srs_freep(packet);
		return ret;
	}
	if (msg->header.is_video())
	{
		g_liveSource->on_publishData(msg,msg->header.stream_id);
	}
	// set to output ppacket only when success.
	*ppacket = packet;

	//srs_freep(packet);
	return ret;
}


int CRtmpProtocolStack::do_decode_message(SrsMessageHeader& header, SrsStream* stream, SrsPacket** ppacket)
{
	int ret = ERROR_SUCCESS;

	SrsPacket* packet = NULL;

	// decode specified packet type
	if (header.is_amf0_command() || header.is_amf3_command() || header.is_amf0_data() || header.is_amf3_data()) {
		srs_verbose("start to decode AMF0/AMF3 command message.");

		// skip 1bytes to decode the amf3 command.
		if (header.is_amf3_command() && stream->require(1)) {
			srs_verbose("skip 1bytes to decode AMF3 command");
			stream->skip(1);
		}

		// amf0 command message.
		// need to read the command name.
		std::string command;
		if ((ret = srs_amf0_read_string(stream, command)) != ERROR_SUCCESS) {
			srs_error("decode AMF0/AMF3 command name failed. ret=%d", ret);
			return ret;
		}
		srs_verbose("AMF0/AMF3 command message, command_name=%s", command.c_str());

		// result/error packet
		if (command == RTMP_AMF0_COMMAND_RESULT || command == RTMP_AMF0_COMMAND_ERROR) {
			double transactionId = 0.0;
			if ((ret = srs_amf0_read_number(stream, transactionId)) != ERROR_SUCCESS) {
				srs_error("decode AMF0/AMF3 transcationId failed. ret=%d", ret);
				return ret;
			}
			srs_verbose("AMF0/AMF3 command id, transcationId=%.2f", transactionId);

			// reset stream, for header read completed.
			stream->skip(-1 * stream->pos());
			if (header.is_amf3_command()) {
				stream->skip(1);
			}

			// find the call name
			if (requests.find(transactionId) == requests.end()) {
				ret = ERROR_RTMP_NO_REQUEST;
				srs_error("decode AMF0/AMF3 request failed. ret=%d", ret);
				return ret;
			}

			std::string request_name = requests[transactionId];
			srs_verbose("AMF0/AMF3 request parsed. request_name=%s", request_name.c_str());

			if (request_name == RTMP_AMF0_COMMAND_CONNECT) {
				srs_info("decode the AMF0/AMF3 response command(%s message).", request_name.c_str());
				*ppacket = packet = new SrsConnectAppResPacket();
				return packet->decode(stream);
			} else if (request_name == RTMP_AMF0_COMMAND_CREATE_STREAM) {
				srs_info("decode the AMF0/AMF3 response command(%s message).", request_name.c_str());
				*ppacket = packet = new SrsCreateStreamResPacket(0, 0);
				return packet->decode(stream);
			} else if (request_name == RTMP_AMF0_COMMAND_RELEASE_STREAM
				|| request_name == RTMP_AMF0_COMMAND_FC_PUBLISH
				|| request_name == RTMP_AMF0_COMMAND_UNPUBLISH) {
					srs_info("decode the AMF0/AMF3 response command(%s message).", request_name.c_str());
					*ppacket = packet = new SrsFMLEStartResPacket(0);
					return packet->decode(stream);
			} else {
				ret = ERROR_RTMP_NO_REQUEST;
				srs_error("decode AMF0/AMF3 request failed. "
					"request_name=%s, transactionId=%.2f, ret=%d", 
					request_name.c_str(), transactionId, ret);
				return ret;
			}
		}

		// reset to zero(amf3 to 1) to restart decode.
		stream->skip(-1 * stream->pos());
		if (header.is_amf3_command()) {
			stream->skip(1);
		}

		// decode command object.
		if (command == RTMP_AMF0_COMMAND_CONNECT) {
			srs_info("decode the AMF0/AMF3 command(connect vhost/app message).");
			*ppacket = packet = new SrsConnectAppPacket();
			 ret = packet->decode(stream);
			 return onConnection(packet);

		} else if(command == RTMP_AMF0_COMMAND_CREATE_STREAM) {
			srs_info("decode the AMF0/AMF3 command(createStream message).");
			*ppacket = packet = new SrsCreateStreamPacket();
			ret = packet->decode(stream);
			return identify_create_stream_client(dynamic_cast<SrsCreateStreamPacket*>(packet),
				res->stream_id,rtmpConnType,req->stream,req->duration);
		} else if(command == RTMP_AMF0_COMMAND_PLAY) {
			srs_info("decode the AMF0/AMF3 command(paly message).");
			*ppacket = packet = new SrsPlayPacket();
			ret = packet->decode(stream);
			return start_play(res->stream_id);
		} else if(command == RTMP_AMF0_COMMAND_PAUSE) {
			srs_info("decode the AMF0/AMF3 command(pause message).");
			*ppacket = packet = new SrsPausePacket();
			return packet->decode(stream);
		} else if(command == RTMP_AMF0_COMMAND_RELEASE_STREAM) {
			srs_info("decode the AMF0/AMF3 command(FMLE releaseStream message).");
			*ppacket = packet = new SrsFMLEStartPacket();
			return packet->decode(stream);
		} else if(command == RTMP_AMF0_COMMAND_FC_PUBLISH) {
			srs_info("decode the AMF0/AMF3 command(FMLE FCPublish message).");
			*ppacket = packet = new SrsFMLEStartPacket();
			return packet->decode(stream);
		} else if(command == RTMP_AMF0_COMMAND_PUBLISH) {
			srs_info("decode the AMF0/AMF3 command(publish message).");
			*ppacket = packet = new SrsPublishPacket();
			ret =  packet->decode(stream);
			return start_flash_publish(res->stream_id);
		} else if(command == RTMP_AMF0_COMMAND_UNPUBLISH) {
			srs_info("decode the AMF0/AMF3 command(unpublish message).");
			*ppacket = packet = new SrsFMLEStartPacket();
			return packet->decode(stream);
		} else if(command == RTMP_AMF0_DATA_SET_DATAFRAME || command == RTMP_AMF0_DATA_ON_METADATA) {
			srs_info("decode the AMF0/AMF3 data(onMetaData message).");
			*ppacket = packet = new SrsOnMetaDataPacket();
			return packet->decode(stream);
		} else if(command == SRS_BW_CHECK_FINISHED
			|| command == SRS_BW_CHECK_PLAYING
			|| command == SRS_BW_CHECK_PUBLISHING
			|| command == SRS_BW_CHECK_STARTING_PLAY
			|| command == SRS_BW_CHECK_STARTING_PUBLISH
			|| command == SRS_BW_CHECK_START_PLAY
			|| command == SRS_BW_CHECK_START_PUBLISH
			|| command == SRS_BW_CHECK_STOPPED_PLAY
			|| command == SRS_BW_CHECK_STOP_PLAY
			|| command == SRS_BW_CHECK_STOP_PUBLISH
			|| command == SRS_BW_CHECK_STOPPED_PUBLISH
			|| command == SRS_BW_CHECK_FINAL)
		{
			srs_info("decode the AMF0/AMF3 band width check message.");
			*ppacket = packet = new SrsBandwidthPacket();
			return packet->decode(stream);
		} else if (command == RTMP_AMF0_COMMAND_CLOSE_STREAM) {
			srs_info("decode the AMF0/AMF3 closeStream message.");
			*ppacket = packet = new SrsCloseStreamPacket();
			return packet->decode(stream);
		} else if (header.is_amf0_command() || header.is_amf3_command()) {
			srs_info("decode the AMF0/AMF3 call message.");
			*ppacket = packet = new SrsCallPacket();
			return packet->decode(stream);
		}

		// default packet to drop message.
		srs_info("drop the AMF0/AMF3 command message, command_name=%s", command.c_str());
		*ppacket = packet = new SrsPacket();
		return ret;
	} else if(header.is_user_control_message()) {
		srs_verbose("start to decode user control message.");
		*ppacket = packet = new SrsUserControlPacket();
		ret = packet->decode(stream);
		return onUserControl(packet);
	} else if(header.is_window_ackledgement_size()) {
		srs_verbose("start to decode set ack window size message.");
		*ppacket = packet = new SrsSetWindowAckSizePacket();
		ret = packet->decode(stream);
		return onAckWindowSize(packet);
	} else if(header.is_set_chunk_size()) {
		srs_verbose("start to decode set chunk size message.");
		*ppacket = packet = new SrsSetChunkSizePacket();
		ret = packet->decode(stream);
		return onSetChunkSize(packet);
	}  else {
		if (!header.is_set_peer_bandwidth() && !header.is_ackledgement()) {
			srs_trace("drop unknown message, type=%d", header.message_type);
		}
	}

	return ret;
}



int CRtmpProtocolStack::do_send_message(SrsMessage* msg, SrsPacket* packet)
{
	int ret = ERROR_SUCCESS;

	// always not NULL msg.
	srs_assert(msg);

	// we donot use the complex basic header,
	// ensure the basic header is 1bytes.
	if (msg->header.perfer_cid < 2) {
		srs_warn("change the chunk_id=%d to default=%d", msg->header.perfer_cid, RTMP_CID_ProtocolControl);
		msg->header.perfer_cid = RTMP_CID_ProtocolControl;
	}

	// p set to current write position,
	// it's ok when payload is NULL and size is 0.
	char* p = msg->payload;
	// to directly set the field.
	char* pp = NULL;

	// always write the header event payload is empty.
	do {
		// generate the header.
		char* pheader = out_header_cache;

		if (p == msg->payload) {
			// write new chunk stream header, fmt is 0
			*pheader++ = 0x00 | (msg->header.perfer_cid & 0x3F);

			// chunk message header, 11 bytes
			// timestamp, 3bytes, big-endian
			u_int32_t timestamp = (u_int32_t)msg->header.timestamp;
			if (timestamp < RTMP_EXTENDED_TIMESTAMP) {
				pp = (char*)&timestamp;
				*pheader++ = pp[2];
				*pheader++ = pp[1];
				*pheader++ = pp[0];
			} else {
				*pheader++ = 0xFF;
				*pheader++ = 0xFF;
				*pheader++ = 0xFF;
			}

			// message_length, 3bytes, big-endian
			pp = (char*)&msg->header.payload_length;
			*pheader++ = pp[2];
			*pheader++ = pp[1];
			*pheader++ = pp[0];

			// message_type, 1bytes
			*pheader++ = msg->header.message_type;

			// message_length, 3bytes, little-endian
			pp = (char*)&msg->header.stream_id;
			*pheader++ = pp[0];
			*pheader++ = pp[1];
			*pheader++ = pp[2];
			*pheader++ = pp[3];

			// chunk extended timestamp header, 0 or 4 bytes, big-endian
			if(timestamp >= RTMP_EXTENDED_TIMESTAMP) {
				pp = (char*)&timestamp;
				*pheader++ = pp[3];
				*pheader++ = pp[2];
				*pheader++ = pp[1];
				*pheader++ = pp[0];
			}
		} else {
			// write no message header chunk stream, fmt is 3
			// @remark, if perfer_cid > 0x3F, that is, use 2B/3B chunk header,
			// SRS will rollback to 1B chunk header.
			*pheader++ = 0xC0 | (msg->header.perfer_cid & 0x3F);

			// chunk extended timestamp header, 0 or 4 bytes, big-endian
			// 6.1.3. Extended Timestamp
			// This field is transmitted only when the normal time stamp in the
			// chunk message header is set to 0x00ffffff. If normal time stamp is
			// set to any value less than 0x00ffffff, this field MUST NOT be
			// present. This field MUST NOT be present if the timestamp field is not
			// present. Type 3 chunks MUST NOT have this field.
			// adobe changed for Type3 chunk:
			//        FMLE always sendout the extended-timestamp,
			//        must send the extended-timestamp to FMS,
			//        must send the extended-timestamp to flash-player.
			// @see: ngx_rtmp_prepare_message
			// @see: http://blog.csdn.net/win_lin/article/details/13363699
			u_int32_t timestamp = (u_int32_t)msg->header.timestamp;
			if (timestamp >= RTMP_EXTENDED_TIMESTAMP) {
				pp = (char*)&timestamp;
				*pheader++ = pp[3];
				*pheader++ = pp[2];
				*pheader++ = pp[1];
				*pheader++ = pp[0];
			}
		}

		// sendout header and payload by writev.
		// decrease the sys invoke count to get higher performance.
		int payload_size = msg->size - (p - msg->payload);
		payload_size = srs_min(payload_size, out_chunk_size);

		// always has header
		int header_size = pheader - out_header_cache;
		srs_assert(header_size > 0);

		// send by writev
		iovec iov[2];
		iov[0].iov_base = out_header_cache;
		iov[0].iov_len = header_size;
		iov[1].iov_base = p;
		iov[1].iov_len = payload_size;

		ssize_t nwrite;
		if ((ret = _io->writev(iov, 2, &nwrite)) != ERROR_SUCCESS) {
			srs_error("send with writev failed. ret=%d", ret);
			return ret;
		}

		// consume sendout bytes when not empty packet.
		if (msg->payload && msg->size > 0) {
			p += payload_size;
		}
	} while (p < msg->payload + msg->size);

	// only process the callback event when with packet
	if (packet && (ret = on_send_packet(msg, packet)) != ERROR_SUCCESS) {
		srs_error("hook the send message failed. ret=%d", ret);
		return ret;
	}

	return ret;
}

int CRtmpProtocolStack::on_send_packet(SrsMessage* msg, SrsPacket* packet)
{
	int ret = ERROR_SUCCESS;

	// should never be raw bytes oriented RTMP message.
	srs_assert(packet);

	switch (msg->header.message_type) {
	case RTMP_MSG_SetChunkSize: {
		SrsSetChunkSizePacket* pkt = dynamic_cast<SrsSetChunkSizePacket*>(packet);
		srs_assert(pkt != NULL);

		out_chunk_size = pkt->chunk_size;

		srs_trace("out chunk size to %d", pkt->chunk_size);
		break;
								}
	case RTMP_MSG_AMF0CommandMessage:
	case RTMP_MSG_AMF3CommandMessage: {
		if (true) {
			SrsConnectAppPacket* pkt = dynamic_cast<SrsConnectAppPacket*>(packet);
			if (pkt) {
				requests[pkt->transaction_id] = pkt->command_name;
				break;
			}
		}
		if (true) {
			SrsCreateStreamPacket* pkt = dynamic_cast<SrsCreateStreamPacket*>(packet);
			if (pkt) {
				requests[pkt->transaction_id] = pkt->command_name;
				break;
			}
		}
		if (true) {
			SrsFMLEStartPacket* pkt = dynamic_cast<SrsFMLEStartPacket*>(packet);
			if (pkt) {
				requests[pkt->transaction_id] = pkt->command_name;
				break;
			}
		}
		break;
								}
	default:
		break;
	}

	return ret;
}

int CRtmpProtocolStack::response_ping_message(int32_t timestamp)
{
	int ret = ERROR_SUCCESS;

	srs_trace("get a ping request, response it. timestamp=%d", timestamp);

	SrsUserControlPacket* pkt = new SrsUserControlPacket();

	pkt->event_type = SrcPCUCPingResponse;
	pkt->event_data = timestamp;

	if ((ret = send_and_free_packet(pkt, 0)) != ERROR_SUCCESS) {
		srs_error("send ping response failed. ret=%d", ret);
		return ret;
	}
	srs_verbose("send ping response success.");

	return ret;
}


int CRtmpProtocolStack::send_and_free_packet(SrsPacket* packet, int stream_id)
{
	int ret = ERROR_SUCCESS;

	srs_assert(packet);
	SrsAutoFree(SrsPacket, packet);

	int size = 0;
	char* payload = NULL;
	if ((ret = packet->encode(size, payload)) != ERROR_SUCCESS) {
		srs_error("encode RTMP packet to bytes oriented RTMP message failed. ret=%d", ret);
		return ret;
	}

	// encode packet to payload and size.
	if (size <= 0 || payload == NULL) {
		srs_warn("packet is empty, ignore empty message.");
		return ret;
	}

	// to message
	SrsMessage* msg = new SrsCommonMessage();

	msg->payload = payload;
	msg->size = size;

	msg->header.payload_length = size;
	msg->header.message_type = packet->get_message_type();
	msg->header.stream_id = stream_id;
	msg->header.perfer_cid = packet->get_prefer_cid();

	// donot use the auto free to free the msg,
	// for performance issue.
	ret = do_send_message(msg, packet);
	srs_freep(msg);

	return ret;
}


int CRtmpProtocolStack::send_and_free_message(SrsMessage* msg, int stream_id)
{
	if (msg) {
		msg->header.stream_id = stream_id;
	}

	// donot use the auto free to free the msg,
	// for performance issue.
	int ret = do_send_message(msg, NULL);
	//srs_freep(msg);
	return ret;
}


//////////////////////////////////////////////////////////////////////////
/////handle protocol message functions
//////////////////////////////////////////////////////////////////////////

int CRtmpProtocolStack::onConnection(SrsPacket* packet)
{
	int ret = ERROR_SUCCESS;

	SrsMessage* msg = NULL;
	SrsConnectAppPacket* pkt = NULL;
	pkt = dynamic_cast<SrsConnectAppPacket*> (packet);
	srs_assert(pkt);
	//if ((ret = expect_message<SrsConnectAppPacket>(&msg, &pkt)) != ERROR_SUCCESS) {
	//	srs_error("expect connect app message failed. ret=%d", ret);
	//	return ret;
	//}

	SrsAutoFree(SrsMessage, msg);
	SrsAutoFree(SrsConnectAppPacket, pkt);
	srs_info("get connect app message");

	SrsAmf0Any* prop = NULL;

	if ((prop = pkt->command_object->ensure_property_string("tcUrl")) == NULL) {
		ret = ERROR_RTMP_REQ_CONNECT;
		srs_error("invalid request, must specifies the tcUrl. ret=%d", ret);
		return ret;
	}
	req->tcUrl = prop->to_str();

	if ((prop = pkt->command_object->ensure_property_string("pageUrl")) != NULL) {
		req->pageUrl = prop->to_str();
	}

	if ((prop = pkt->command_object->ensure_property_string("swfUrl")) != NULL) {
		req->swfUrl = prop->to_str();
	}

	if ((prop = pkt->command_object->ensure_property_number("objectEncoding")) != NULL) {
		req->objectEncoding = prop->to_number();
	}

	if (pkt->args) {
		srs_freep(req->args);
		req->args = pkt->args->copy()->to_object();
		srs_info("copy edge traverse to origin auth args.");
	}

	srs_info("get connect app message params success.");

	srs_discovery_tc_url(req->tcUrl, 
		req->schema, req->host, req->vhost, req->app, req->port,
		req->param);
	req->strip();


	srs_info("discovery app success. schema=%s, vhost=%s, port=%s, app=%s",
		req->schema.c_str(), req->vhost.c_str(), req->port.c_str(), req->app.c_str());

	if (req->schema.empty() || req->vhost.empty() || req->port.empty() || req->app.empty()) {
		//ret = ERROR_RTMP_REQ_TCURL;
		srs_error("discovery tcUrl failed. "
			"tcUrl=%s, schema=%s, vhost=%s, port=%s, app=%s, ret=%d",
			req->tcUrl.c_str(), req->schema.c_str(), req->vhost.c_str(), req->port.c_str(), req->app.c_str(), ret);
		//return ret;
	}

	//if ((ret = check_vhost()) != ERROR_SUCCESS) {
	//	srs_error("check vhost failed. ret=%d", ret);
	//	return ret;
	//}
	srs_verbose("check vhost success.");

	srs_trace("connect app, "
		"tcUrl=%s, pageUrl=%s, swfUrl=%s, schema=%s, vhost=%s, port=%s, app=%s, args=%s", 
		req->tcUrl.c_str(), req->pageUrl.c_str(), req->swfUrl.c_str(), 
		req->schema.c_str(), req->vhost.c_str(), req->port.c_str(),
		req->app.c_str(), (req->args? "(obj)":"null"));

	// show client identity
	if(req->args) {
		std::string srs_version;
		std::string srs_server_ip;
		int srs_pid = 0;
		int srs_id = 0;

		SrsAmf0Any* prop = NULL;
		if ((prop = req->args->ensure_property_string("srs_version")) != NULL) {
			srs_version = prop->to_str();
		}
		if ((prop = req->args->ensure_property_string("srs_server_ip")) != NULL) {
			srs_server_ip = prop->to_str();
		}
		if ((prop = req->args->ensure_property_number("srs_pid")) != NULL) {
			srs_pid = (int)prop->to_number();
		}
		if ((prop = req->args->ensure_property_number("srs_id")) != NULL) {
			srs_id = (int)prop->to_number();
		}

		srs_info("edge-srs ip=%s, version=%s, pid=%d, id=%d", 
			srs_server_ip.c_str(), srs_version.c_str(), srs_pid, srs_id);
		if (srs_pid > 0) {
			srs_trace("edge-srs ip=%s, version=%s, pid=%d, id=%d", 
				srs_server_ip.c_str(), srs_version.c_str(), srs_pid, srs_id);
		}
	}

	ret = set_window_ack_size(2.5 * 1000 * 1000);

	ret = set_peer_bandwidth(2.5 * 1000 * 1000,2);

	ret = set_chunk_size(out_chunk_size);

	string localIp = "192.168.0.9";
	ret = response_connect_app(req,localIp.c_str());

	return ret;
}

int  CRtmpProtocolStack::onSetChunkSize(SrsPacket* packet)
{
	SrsSetChunkSizePacket* pkt = dynamic_cast<SrsSetChunkSizePacket*>(packet);
	srs_assert(pkt != NULL);

	// for some server, the actual chunk size can greater than the max value(65536),
	// so we just warning the invalid chunk size, and actually use it is ok,
	// @see: https://github.com/winlinvip/simple-rtmp-server/issues/160
	if (pkt->chunk_size < SRS_CONSTS_RTMP_MIN_CHUNK_SIZE 
		|| pkt->chunk_size > SRS_CONSTS_RTMP_MAX_CHUNK_SIZE) 
	{
		srs_warn("accept chunk size %d, but should in [%d, %d], "
			"@see: https://github.com/winlinvip/simple-rtmp-server/issues/160",
			pkt->chunk_size, SRS_CONSTS_RTMP_MIN_CHUNK_SIZE, 
			SRS_CONSTS_RTMP_MAX_CHUNK_SIZE);
	}

	in_chunk_size = pkt->chunk_size;

	srs_trace("input chunk size to %d", pkt->chunk_size);

	return ERROR_SUCCESS;
	 
}
int  CRtmpProtocolStack::onAckWindowSize(SrsPacket* packet)
{	
	SrsSetWindowAckSizePacket* pkt = dynamic_cast<SrsSetWindowAckSizePacket*>(packet);
	srs_assert(pkt != NULL);

	if (pkt->ackowledgement_window_size > 0) {
		in_ack_size.ack_window_size = pkt->ackowledgement_window_size;
		// @remakr, we ignore this message, for user noneed to care.
		// but it's important for dev, for client/server will block if required 
		// ack msg not arrived.
		srs_info("set ack window size to %d", pkt->ackowledgement_window_size);
	} else {
		srs_warn("ignored. set ack window size is %d", pkt->ackowledgement_window_size);
	}
	return ERROR_SUCCESS;
}

int CRtmpProtocolStack::onUserControl(SrsPacket* packet)
{
	int ret = ERROR_SUCCESS;
	SrsUserControlPacket* pkt = dynamic_cast<SrsUserControlPacket*>(packet);
	srs_assert(pkt != NULL);

	if (pkt->event_type == SrcPCUCSetBufferLength) {
		srs_trace("ignored. set buffer length to %d", pkt->extra_data);
	}
	if (pkt->event_type == SrcPCUCPingRequest) {
		if ((ret = response_ping_message(pkt->event_data)) != ERROR_SUCCESS) {
			return ret;
		}
	}
	return ret;
}



int CRtmpProtocolStack::set_window_ack_size(int ack_size)
{
	CRtmpProtocolStack* protocol = this;
	int ret = ERROR_SUCCESS;

	SrsSetWindowAckSizePacket* pkt = new SrsSetWindowAckSizePacket();
	pkt->ackowledgement_window_size = ack_size;
	if ((ret = protocol->send_and_free_packet(pkt, 0)) != ERROR_SUCCESS) {
		srs_error("send ack size message failed. ret=%d", ret);
		return ret;
	}
	srs_info("send ack size message success. ack_size=%d", ack_size);

	return ret;
}


int CRtmpProtocolStack::set_peer_bandwidth(int bandwidth, int type)
{
	CRtmpProtocolStack* protocol = this;
	int ret = ERROR_SUCCESS;

	SrsSetPeerBandwidthPacket* pkt = new SrsSetPeerBandwidthPacket();
	pkt->bandwidth = bandwidth;
	pkt->type = type;
	if ((ret = protocol->send_and_free_packet(pkt, 0)) != ERROR_SUCCESS) {
		srs_error("send set bandwidth message failed. ret=%d", ret);
		return ret;
	}
	srs_info("send set bandwidth message "
		"success. bandwidth=%d, type=%d", bandwidth, type);

	return ret;
}

int CRtmpProtocolStack::response_connect_app(SrsRequest *req, const char* server_ip)
{
	CRtmpProtocolStack* protocol = this;
	int ret = ERROR_SUCCESS;

	SrsConnectAppResPacket* pkt = new SrsConnectAppResPacket();

	pkt->props->set("fmsVer", SrsAmf0Any::str("FMS/"RTMP_SIG_FMS_VER));
	pkt->props->set("capabilities", SrsAmf0Any::number(127));
	pkt->props->set("mode", SrsAmf0Any::number(1));

	pkt->info->set(StatusLevel, SrsAmf0Any::str(StatusLevelStatus));
	pkt->info->set(StatusCode, SrsAmf0Any::str(StatusCodeConnectSuccess));
	pkt->info->set(StatusDescription, SrsAmf0Any::str("Connection succeeded"));
	pkt->info->set("objectEncoding", SrsAmf0Any::number(req->objectEncoding));
	SrsAmf0EcmaArray* data = SrsAmf0Any::ecma_array();
	pkt->info->set("data", data);

	data->set("version", SrsAmf0Any::str(RTMP_SIG_FMS_VER));
	data->set("srs_sig", SrsAmf0Any::str(RTMP_SIG_SRS_KEY));
	data->set("srs_server", SrsAmf0Any::str(RTMP_SIG_SRS_SERVER));
	data->set("srs_license", SrsAmf0Any::str(RTMP_SIG_SRS_LICENSE));
	data->set("srs_role", SrsAmf0Any::str(RTMP_SIG_SRS_ROLE));
	data->set("srs_url", SrsAmf0Any::str(RTMP_SIG_SRS_URL));
	data->set("srs_version", SrsAmf0Any::str(RTMP_SIG_SRS_VERSION));
	data->set("srs_site", SrsAmf0Any::str(RTMP_SIG_SRS_WEB));
	data->set("srs_email", SrsAmf0Any::str(RTMP_SIG_SRS_EMAIL));
	data->set("srs_copyright", SrsAmf0Any::str(RTMP_SIG_SRS_COPYRIGHT));
	data->set("srs_primary", SrsAmf0Any::str(RTMP_SIG_SRS_PRIMARY));
	data->set("srs_authors", SrsAmf0Any::str(RTMP_SIG_SRS_AUTHROS));

	if (server_ip) {
		data->set("srs_server_ip", SrsAmf0Any::str(server_ip));
	}
	// for edge to directly get the id of client.
	data->set("srs_pid", SrsAmf0Any::number(getpid()));
	data->set("srs_id", SrsAmf0Any::number(_srs_context->get_id()));

	if ((ret = protocol->send_and_free_packet(pkt, 0)) != ERROR_SUCCESS) {
		srs_error("send connect app response message failed. ret=%d", ret);
		return ret;
	}
	srs_info("send connect app response message success.");

	return ret;
}


int CRtmpProtocolStack::identify_create_stream_client(SrsCreateStreamPacket* req, int stream_id, SrsRtmpConnType& type, string& stream_name, double& duration)
{
	int ret = ERROR_SUCCESS;
	CRtmpProtocolStack* protocol = this;
	srs_info("identify client by create stream, play or flash publish.");
	if (true) {
		SrsCreateStreamResPacket* pkt = new SrsCreateStreamResPacket(req->transaction_id, stream_id);
		if ((ret = protocol->send_and_free_packet(pkt, 0)) != ERROR_SUCCESS) {
			srs_error("send createStream response message failed. ret=%d", ret);
			return ret;
		}
		srs_info("send createStream response message success.");
	}

	return ret;
}


int CRtmpProtocolStack::identify_play_client(SrsPlayPacket* req, SrsRtmpConnType& type, string& stream_name, double& duration)
{
	int ret = ERROR_SUCCESS;

	type = SrsRtmpConnPlay;
	stream_name = req->stream_name;
	duration = req->duration;

	srs_info("identity client type=play, stream_name=%s, duration=%.2f", stream_name.c_str(), duration);

	this->req->strip();
	srs_trace("client identified, type=%s, stream_name=%s, duration=%.2f", 
		srs_client_type_string(type).c_str(), this->req->stream.c_str(), this->req->duration);

	return ret;
}

int CRtmpProtocolStack::identify_flash_publish_client(SrsPublishPacket* req, SrsRtmpConnType& type, string& stream_name)
{
	int ret = ERROR_SUCCESS;

	type = SrsRtmpConnFlashPublish;
	stream_name = req->stream_name;

	this->req->strip();
	srs_trace("client identified, type=%s, stream_name=%s, duration=%.2f", 
		srs_client_type_string(type).c_str(), this->req->stream.c_str(), this->req->duration);

	return ret;
}


int CRtmpProtocolStack::identify_fmle_publish_client(SrsFMLEStartPacket* req, SrsRtmpConnType& type, string& stream_name)
{
	int ret = ERROR_SUCCESS;
	CRtmpProtocolStack* protocol = this;
	type = SrsRtmpConnFMLEPublish;
	stream_name = req->stream_name;

	// releaseStream response
	if (true) {
		SrsFMLEStartResPacket* pkt = new SrsFMLEStartResPacket(req->transaction_id);
		if ((ret = protocol->send_and_free_packet(pkt, 0)) != ERROR_SUCCESS) {
			srs_error("send releaseStream response message failed. ret=%d", ret);
			return ret;
		}
		srs_info("send releaseStream response message success.");
	}

	this->req->strip();
	srs_trace("client identified, type=%s, stream_name=%s, duration=%.2f", 
		srs_client_type_string(type).c_str(), this->req->stream.c_str(), this->req->duration);

	return ret;
}


int CRtmpProtocolStack::start_play(int stream_id)
{
	int ret = ERROR_SUCCESS;
	CRtmpProtocolStack* protocol = this;

	//set chunk size
	int chunk_size = 128;
	if ((ret = this->set_chunk_size(chunk_size)) != ERROR_SUCCESS) {
		srs_error("set chunk_size=%d failed. ret=%d", chunk_size, ret);
		return ret;
	}
	srs_info("set chunk_size=%d success", chunk_size);


	// StreamBegin
	if (true) {
		SrsUserControlPacket* pkt = new SrsUserControlPacket();
		pkt->event_type = SrcPCUCStreamBegin;
		pkt->event_data = stream_id;
		if ((ret = protocol->send_and_free_packet(pkt, 0)) != ERROR_SUCCESS) {
			srs_error("send PCUC(StreamBegin) message failed. ret=%d", ret);
			return ret;
		}
		srs_info("send PCUC(StreamBegin) message success.");
	}

	// onStatus(NetStream.Play.Reset)
	if (true) {
		SrsOnStatusCallPacket* pkt = new SrsOnStatusCallPacket();

		pkt->data->set(StatusLevel, SrsAmf0Any::str(StatusLevelStatus));
		pkt->data->set(StatusCode, SrsAmf0Any::str(StatusCodeStreamReset));
		pkt->data->set(StatusDescription, SrsAmf0Any::str("Playing and resetting stream."));
		pkt->data->set(StatusDetails, SrsAmf0Any::str("stream"));
		pkt->data->set(StatusClientId, SrsAmf0Any::str(RTMP_SIG_CLIENT_ID));

		if ((ret = protocol->send_and_free_packet(pkt, stream_id)) != ERROR_SUCCESS) {
			srs_error("send onStatus(NetStream.Play.Reset) message failed. ret=%d", ret);
			return ret;
		}
		srs_info("send onStatus(NetStream.Play.Reset) message success.");
	}

	// onStatus(NetStream.Play.Start)
	if (true) {
		SrsOnStatusCallPacket* pkt = new SrsOnStatusCallPacket();

		pkt->data->set(StatusLevel, SrsAmf0Any::str(StatusLevelStatus));
		pkt->data->set(StatusCode, SrsAmf0Any::str(StatusCodeStreamStart));
		pkt->data->set(StatusDescription, SrsAmf0Any::str("Started playing stream."));
		pkt->data->set(StatusDetails, SrsAmf0Any::str("stream"));
		pkt->data->set(StatusClientId, SrsAmf0Any::str(RTMP_SIG_CLIENT_ID));

		if ((ret = protocol->send_and_free_packet(pkt, stream_id)) != ERROR_SUCCESS) {
			srs_error("send onStatus(NetStream.Play.Reset) message failed. ret=%d", ret);
			return ret;
		}
		srs_info("send onStatus(NetStream.Play.Reset) message success.");
	}

	// |RtmpSampleAccess(false, false)
	if (true) {
		SrsSampleAccessPacket* pkt = new SrsSampleAccessPacket();

		// allow audio/video sample.
		// @see: https://github.com/winlinvip/simple-rtmp-server/issues/49
		pkt->audio_sample_access = true;
		pkt->video_sample_access = true;

		if ((ret = protocol->send_and_free_packet(pkt, stream_id)) != ERROR_SUCCESS) {
			srs_error("send |RtmpSampleAccess(false, false) message failed. ret=%d", ret);
			return ret;
		}
		srs_info("send |RtmpSampleAccess(false, false) message success.");
	}

	// onStatus(NetStream.Data.Start)
	if (true) {
		SrsOnStatusDataPacket* pkt = new SrsOnStatusDataPacket();
		pkt->data->set(StatusCode, SrsAmf0Any::str(StatusCodeDataStart));
		if ((ret = protocol->send_and_free_packet(pkt, stream_id)) != ERROR_SUCCESS) {
			srs_error("send onStatus(NetStream.Data.Start) message failed. ret=%d", ret);
			return ret;
		}
		srs_info("send onStatus(NetStream.Data.Start) message success.");
	}

	srs_info("start play success.");
	g_liveSource->addSubscriber(this);
	return ret;
}


int CRtmpProtocolStack::set_chunk_size(int chunk_size)
{
	int ret = ERROR_SUCCESS;
	CRtmpProtocolStack* protocol = this;
	SrsSetChunkSizePacket* pkt = new SrsSetChunkSizePacket();
	pkt->chunk_size = chunk_size;
	if ((ret = protocol->send_and_free_packet(pkt, 0)) != ERROR_SUCCESS) {
		srs_error("send set chunk size message failed. ret=%d", ret);
		return ret;
	}
	srs_info("send set chunk size message success. chunk_size=%d", chunk_size);

	return ret;
}


int CRtmpProtocolStack::start_flash_publish(int stream_id)
{
	int ret = ERROR_SUCCESS;
	CRtmpProtocolStack* protocol = this;
	// publish response onStatus(NetStream.Publish.Start)
	if (true) {
		SrsOnStatusCallPacket* pkt = new SrsOnStatusCallPacket();

		pkt->data->set(StatusLevel, SrsAmf0Any::str(StatusLevelStatus));
		pkt->data->set(StatusCode, SrsAmf0Any::str(StatusCodePublishStart));
		pkt->data->set(StatusDescription, SrsAmf0Any::str("Started publishing stream."));
		pkt->data->set(StatusClientId, SrsAmf0Any::str(RTMP_SIG_CLIENT_ID));

		if ((ret = protocol->send_and_free_packet(pkt, stream_id)) != ERROR_SUCCESS) {
			srs_error("send onStatus(NetStream.Publish.Start) message failed. ret=%d", ret);
			return ret;
		}
		srs_info("send onStatus(NetStream.Publish.Start) message success.");
	}

	srs_info("flash publish success.");

	return ret;
}


