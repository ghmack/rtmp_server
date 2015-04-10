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