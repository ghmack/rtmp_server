#include "NetServer.h"
#include <stdio.h>

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
	if (err)
	{
		info_trace("%s io operate error\r\n",bReadOpt?"read":"write");
	}
	bReadOpt?_recvSize += size:_sendSize += size;
	if (funBack==NULL)
	{
		if(err)
			ThrExp(bReadOpt?"read io operate error\r\n":"write io operate error\r\n");
		return ;
	}
	funBack(size,!err);
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
	if (err)
	{
		ThrExp("recv message error");
	}
	in_buffer->append(_buffer,size);

	while(true)
	{
		readBasicChunkHeader();
		readMsgHeader();
		readMsgPayload();
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
		if (!chunk->msg) {
			chunk->msg = new SrsCommonMessage();
			srs_verbose("create message for new chunk, fmt=%d, cid=%d", fmt, chunk->cid);
		}

		static char mh_sizes[] = {11,7,3,0};
		char mh_size = mh_sizes[fmt];
		char* p = in_buffer->bytes();
		if (in_buffer->length() >= mh_size)
		{
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
				chunk->header.payload_length, chunk->header.timestamp, chunk->headr.stream_id);

			//onMessagePop(); //igore empty messages
			srs_freep(chunk->msg);
			chunk->msg = NULL;
			chunk->msg_count =0;
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

			srs_verbose("chunk payload read completed. bh_size=%d, mh_size=%d, payload_size=%d", bh_size, mh_size, payload_size);

			// got entire RTMP message?
			if (chunk->header.payload_length == chunk->msg->size) {							
				srs_verbose("get entire RTMP message(type=%d, size=%d, time=%"PRId64", sid=%d)", 
					chunk->header.message_type, chunk->header.payload_length, 
					chunk->header.timestamp, chunk->header.stream_id);
				onInnerRecvMessage(chunk->msg);
				chunk->msg = NULL;
				chunk->msg_count =0;
				_decode_state = decode_init;
				return ;
			}

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

void CRtmpProtocolStack::onInnerRecvMessage(SrsMessage* msg)
{
	srs_assert(msg);

	if (msg->size <= 0 || msg->header.payload_length <= 0) {
		srs_trace("ignore empty message(type=%d, size=%d, time=%"PRId64", sid=%d).",
			msg->header.message_type, msg->header.payload_length,
			msg->header.timestamp, msg->header.stream_id);
		srs_freep(msg);
		return;
	}






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

	// set to output ppacket only when success.
	*ppacket = packet;

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
			return packet->decode(stream);
		} else if(command == RTMP_AMF0_COMMAND_CREATE_STREAM) {
			srs_info("decode the AMF0/AMF3 command(createStream message).");
			*ppacket = packet = new SrsCreateStreamPacket();
			return packet->decode(stream);
		} else if(command == RTMP_AMF0_COMMAND_PLAY) {
			srs_info("decode the AMF0/AMF3 command(paly message).");
			*ppacket = packet = new SrsPlayPacket();
			return packet->decode(stream);
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
			return packet->decode(stream);
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
		return packet->decode(stream);
	} else if(header.is_window_ackledgement_size()) {
		srs_verbose("start to decode set ack window size message.");
		*ppacket = packet = new SrsSetWindowAckSizePacket();
		return packet->decode(stream);
	} else if(header.is_set_chunk_size()) {
		srs_verbose("start to decode set chunk size message.");
		*ppacket = packet = new SrsSetChunkSizePacket();
		return packet->decode(stream);
	} else {
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