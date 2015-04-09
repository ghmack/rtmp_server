#pragma once


#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <string>
#include <set>
using namespace std;

#define boost_error boost::system::error_code 

#define info_trace printf

#define IO_READ_BUFFER_SIZE 4096

inline void ThrowException(const char* exp) 
{ 
	char buffer[1000];
	sprintf(buffer,"%s,File: %s, Line: %s",exp,__FILE__,__LINE__);
	printf(buffer);
	throw buffer;
}

#define ThrExp(s) ThrowException(s) 


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

public:
	void onIO(int size, boost::system::error_code err,boost::function<void (int,bool)> funBack,bool bReadOpt);

private:
	boost::asio::ip::tcp::socket& _socket;

};





class IRtmpListener
{
public:
	virtual void onMessagePop() = 0;
};

class CRtmpProtocolStack
{
public:
	CRtmpProtocolStack(CReadWriteIO* io);
	~CRtmpProtocolStack();

	void addListener(IRtmpListener* listener);
	void pushMessage();
	void open();

private:
	CReadWriteIO* _io;
	IRtmpListener* _listener;
	

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

