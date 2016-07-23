#include "RtmpDecode.h"






CRtmpHandshake::CRtmpHandshake():m_state(hs_state_init)
{

}

CRtmpHandshake::~CRtmpHandshake()
{

}

CRtmpHandshake::eum_state_hs CRtmpHandshake::state()
{
	return m_state;
}


int CRtmpHandshake::create_s0s1s2(const char* c1, const char* c0c1,char* s0s1s2)
{
	int ret = ERROR_SUCCESS;

	if (!s0s1s2) {
		return ret;
	}

	//s0s1s2 = new char[3073];
	srs_random_generate(s0s1s2, 3073);

	// plain text required.
	SrsStream stream;
	if ((ret = stream.initialize(s0s1s2, 9)) != ERROR_SUCCESS) {
		return ret;
	}
	stream.write_1bytes(0x03);
	stream.write_4bytes(::time(NULL));
	// s2 time2 copy from c1
	if (c0c1) {
		stream.write_bytes((char*)c0c1 + 1, 4);
	}

	// if c1 specified, copy c1 to s2.
	// @see: https://github.com/winlinvip/simple-rtmp-server/issues/46
	if (c1) {
		memcpy(s0s1s2 + 1537, c1, 1536);
	}

	return ret;
}


void CRtmpHandshake::handshakeWithClient(char* data, int size,
	handshakeResponse writeData,void* param,
	onHandshakeSuccess onSuccess, void* param2)
{
	if (m_state == hs_state_init)
	{
		m_c0c1.append(data,size);
		if (m_c0c1.size() == 1537)
		{
			char* c0c1 = (char*)m_c0c1.data();
			m_state = hs_state_c2;
			char s0s1s2[3073] = {0};
			do 
			{
				int ret = ERROR_SUCCESS;

				ssize_t nsize;


				// decode c1
				c1s1 c1;
				// try schema0.
				if ((ret = c1.parse(c0c1 + 1, srs_schema0)) != ERROR_SUCCESS) {
					srs_error("parse c1 schema%d error. ret=%d", srs_schema0, ret);
					break;;
				}
				// try schema1
				bool is_valid = false;
				if ((ret = c1.c1_validate_digest(is_valid)) != ERROR_SUCCESS || !is_valid) {
					if ((ret = c1.parse(c0c1 + 1, srs_schema1)) != ERROR_SUCCESS) {
						srs_error("parse c1 schema%d error. ret=%d", srs_schema1, ret);
						break;
					}

					if ((ret = c1.c1_validate_digest(is_valid)) != ERROR_SUCCESS || !is_valid) {
						ret = ERROR_RTMP_TRY_SIMPLE_HS;
						srs_info("all schema valid failed, try simple handshake. ret=%d", ret);
						break;;
					}
				}
				srs_verbose("decode c1 success.");

				// encode s1
				c1s1 s1;
				if ((ret = s1.s1_create(&c1)) != ERROR_SUCCESS) {
					srs_error("create s1 from c1 failed. ret=%d", ret);
					break;
				}
				srs_verbose("create s1 from c1 success.");
				// verify s1
				if ((ret = s1.s1_validate_digest(is_valid)) != ERROR_SUCCESS || !is_valid) {
					ret = ERROR_RTMP_TRY_SIMPLE_HS;
					srs_info("verify s1 failed, try simple handshake. ret=%d", ret);
					break;
				}
				srs_verbose("verify s1 success.");

				c2s2 s2;
				if ((ret = s2.s2_create(&c1)) != ERROR_SUCCESS) {
					srs_error("create s2 from c1 failed. ret=%d", ret);
					break;
				}
				srs_verbose("create s2 from c1 success.");
				// verify s2
				if ((ret = s2.s2_validate(&c1, is_valid)) != ERROR_SUCCESS || !is_valid) {
					ret = ERROR_RTMP_TRY_SIMPLE_HS;
					srs_info("verify s2 failed, try simple handshake. ret=%d", ret);
					break;
				}
				srs_verbose("verify s2 success.");

				// sendout s0s1s2
				if ((ret = create_s0s1s2(NULL,m_c0c1.data(),s0s1s2)) != ERROR_SUCCESS) {
					break;
				}
				s1.dump(s0s1s2 + 1);
				s2.dump(s0s1s2 + 1537);
				if ((ret = writeData(s0s1s2, 3073,param)) != ERROR_SUCCESS) {
					srs_warn("complex handshake send s0s1s2 failed. ret=%d", ret);
					break;
				}
				srs_verbose("complex handshake send s0s1s2 success.");

				return ;

			} while (0);

			s0s1s2[0] = 0x03;
			uint32_t tm = time(NULL);
			memcpy(s0s1s2 + 1,&tm,4);
			memcpy(s0s1s2 + 5,m_c0c1.data() + 1,4);
			memcpy(s0s1s2 + 1537,m_c0c1.data(),1536);
			if(writeData((void*)s0s1s2,3073,param) != ERROR_SUCCESS)
			{
				srs_warn("simple handshake send s0s1s2 failed");
				return;
			}
			m_state = hs_state_c2;
		}
	}
	else if (m_state == hs_state_c2)
	{
		if(size >= 3073)
		{
			m_c2.append(data + 1537,size - 1537);
			m_state = hs_state_successed;
			onHandshakeSuccess(param2);
			return ;
		}
	}

}


//////////////////////////////////////////////////////////////////////////
RtmpProtocolstack::RtmpProtocolstack(SrsBuffer2* decodeBuffer):in_buffer(decodeBuffer)
{

}


RtmpProtocolstack::~RtmpProtocolstack()
{

}

int RtmpProtocolstack::readBasicChunkHeader()
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
				srs_error("decode chunk id error ,cid < 1");
				return -1;
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
		else //if (in_buffer->length() < 1)
		{	
			_wait_buffer = true; 
		}
	}

	return ERROR_SUCCESS;
}


int RtmpProtocolstack::readMsgHeader()
{
	if (_decode_state == decode_mh)
	{
		if(chunk_streams.find(_current_cid)==chunk_streams.end())
		{	
			srs_error("find cid %d error",_current_cid);
			return -1;
		}
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
				return -1;
			}
		}

		// when exists cache msg, means got an partial message,
		// the fmt must not be type0 which means new message.
		if (chunk->msg && fmt == RTMP_FMT_TYPE0) {
			int ret = ERROR_RTMP_CHUNK_START;
			srs_error("chunk stream exists, "
				"fmt must not be %d, actual is %d. ret=%d", RTMP_FMT_TYPE0, fmt, ret);
			return -1;
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
						return -1;
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
			{
				srs_error("find cid %d error",_current_cid);
				return -1;
			}
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

	return ERROR_SUCCESS;
}


int RtmpProtocolstack::readMsgPayload()
{
	if (_decode_state == decode_payload)
	{
		if(chunk_streams.find(_current_cid)==chunk_streams.end())
		{	
			srs_error("find cid %d error",_current_cid);
			return -1;
		}
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
			return ERROR_SUCCESS;
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
				return ERROR_SUCCESS;
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
	return ERROR_SUCCESS;
}

int RtmpProtocolstack::onInnerRecvMessage(SrsMessage* msg)
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
	return decode_message(msg, &ppacket);
}

int  RtmpProtocolstack::decode_message(SrsMessage* msg, SrsPacket** ppacket)
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
	if (msg->header.is_video() || msg->header.is_audio())
	{
		//g_liveSource->on_publishData(msg,msg->header.stream_id);
	}
	// set to output ppacket only when success.
	*ppacket = packet;

	//srs_freep(packet);
	return ret;
}


int  RtmpProtocolstack::do_decode_message(SrsMessageHeader& header, SrsStream* stream, SrsPacket** ppacket)
{
	do 
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
				break;
			}
			srs_verbose("AMF0/AMF3 command message, command_name=%s", command.c_str());

			// result/error packet
			if (command == RTMP_AMF0_COMMAND_RESULT || command == RTMP_AMF0_COMMAND_ERROR) {
				double transactionId = 0.0;
				if ((ret = srs_amf0_read_number(stream, transactionId)) != ERROR_SUCCESS) {
					srs_error("decode AMF0/AMF3 transcationId failed. ret=%d", ret);
					break;
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
					break;;
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
					break;
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


	} while (0);
}