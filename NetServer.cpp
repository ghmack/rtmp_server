#include "NetServer.h"
#include <stdio.h>

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
RtmpConnection::RtmpConnection(boost::asio::io_service& ios):_socket(ios)
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