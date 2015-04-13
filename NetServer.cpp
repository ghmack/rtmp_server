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
		char* p = _inBuffer->bytes();
		if (_inBuffer->length() >= 1)
		{			
			fmt = (*p >> 6) & 0x03;
			cid = *p & 0x3f;
			if (cid > 1)
			{
				_decode_state = decode_mh;
			}
			else if (cid == 1)
			{
				if (_inBuffer->length() >=2 )
				{
					cid = 64;
					cid += (u_int8_t)*(++p);
					bh_size = 2;
					_decode_state = decode_mh;
				}
			}
			else if (cid == 0)
			{
				if (_inBuffer->length() >=3)
				{
					cid = 64;
					cid += (u_int8_t)*(++p);
					cid += ((u_int8_t)*(++p)) * 256;
					bh_size = 3;
					_decode_state = decode_mh;
				}
			}
			else
			{
				srs_assert(0);
			}
			srs_verbose("read basic header success. fmt=%d, cid=%d, bh_size=%d", fmt, cid, bh_size);

			if (_decode_state == decode_mh)
			{
				_current_cid = cid;
				_inBuffer->erase(bh_size);
				if (_mapChunkStream.find(cid) == _mapChunkStream.end())
				{
					_mapChunkStream[cid] = new SrsChunkStream(cid);
					_mapChunkStream[cid]->fmt = fmt;
				}
			}
		}
	}
}
void CRtmpProtocolStack::recvMessage(int size, bool err)
{
	if (err)
	{
		ThrExp("recv message error");
	}
	_inBuffer->append(_buffer,size);

	readBasicChunkHeader();

	readMsgHeader();



}


void CRtmpProtocolStack::readMsgHeader()
{
	if (_decode_state == decode_mh)
	{
		if(_mapChunkStream.find(_current_cid)==_mapChunkStream.end())
			ThrExp("find cid %d error",_current_cid);
		SrsChunkStream* chunk = _mapChunkStream[_current_cid];
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
		char* p = _inBuffer->bytes();
		if (_inBuffer->length() >= mh_size)
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
			_inBuffer->erase(mh_size);
		}

	}

	if (_decode_state == decode_ext_time)
	{
		if (_inBuffer->length() >= 4)
		{
			if(_mapChunkStream.find(_current_cid)==_mapChunkStream.end())
				ThrExp("find cid %d error",_current_cid);
			SrsChunkStream* chunk = _mapChunkStream[_current_cid];
			char fmt = chunk->fmt;
			bool is_first_chunk_of_msg = !chunk->msg;
			char* p = _inBuffer->bytes();

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
			_inBuffer->erase(4);
			chunk->header.timestamp &= 0x7fffffff;
			srs_assert(chunk->header.payload_length >= 0);
			chunk->msg->header = chunk->header;
			// increase the msg count, the chunk stream can accept fmt=1/2/3 message now.
			chunk->msg_count++;
		}
	}

	return ;
}


void CRtmpProtocolStack::readMsgPayload()
{
	if (_decode_state == decode_payload)
	{
		if(_mapChunkStream.find(_current_cid)==_mapChunkStream.end())
			ThrExp("find cid %d error",_current_cid);
		SrsChunkStream* chunk = _mapChunkStream[_current_cid];


	}
	
}

