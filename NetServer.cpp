#include "NetServer.h"
#include <stdio.h>

CReadWriteIO::CReadWriteIO(boost::asio::ip::tcp::socket& socket):_socket(socket)
{

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


void CReadWriteIO::onIO(int size, boost::system::error_code err,boost::function<void (int,bool)> funBack,bool bReadOpt)
{
	if (!err)
	{
		info_trace("%s io operate error\r\n",bReadOpt?"read":"write");
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