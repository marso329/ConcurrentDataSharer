#ifndef CONCURRENTDATASHARER_H_
#define CONCURRENTDATASHARER_H_
#include <string>
#include <sstream>
#include <cstddef> // NULL
#include <iomanip>
#include <iostream>
#include <fstream>
#include <string>
#include <boost/archive/tmpdir.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/assume_abstract.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include "boost/date_time/posix_time/posix_time_types.hpp"

#include "structures.h"
#include "BlockingQueue.h"
#include <unordered_map>

class ConcurrentDataSharer{
public:
	ConcurrentDataSharer(std::string const &);
	~ConcurrentDataSharer();
	template<typename T>
	void set(std::string const& name,T data){
		std::ostringstream ss;
		boost::archive::text_oarchive ar(ss);
		ar<<data;
		QueueElementSet* element=new QueueElementSet(name,ss.str());
		_TCPRecvQueue->Put(element);

	}

	template<typename T>
	T get(std::string const& name){
		QueueElementGet* element=new QueueElementGet(name);
		_TCPRecvQueue->Put(element);
		std::string data= element->getData();
		std::istringstream ar(data);
		boost::archive::text_iarchive ia(ar);
		T returnData;
		ia>>returnData;
		return returnData;
	}

protected:
private:
	//thread functions
	void mainLoop();
	void TCPRecv();
	void TCPSend();
	void MultiSend();
	void MultiRecv();

	//handle functions
	void handleTCPRecv(QueueElementBase*);
	void handleMultiRecv(QueueElementBase*);
	void handleMultiRecvRaw(const boost::system::error_code& error,
		      size_t bytes_recvd);

	void handleMultiSendRaw(const boost::system::error_code& error);
	void handleMultiSendTimeout(const boost::system::error_code& error);

	std::string _groupName;
BlockingQueue<QueueElementBase*>* _multiSendQueue;
BlockingQueue<QueueElementBase*>* _multiRecvQueue;
BlockingQueue<QueueElementBase*>* _TCPSendQueue;
BlockingQueue<QueueElementBase*>* _TCPRecvQueue;
boost::thread* _mainThread;
boost::thread* _TCPSendThread;
boost::thread* _TCPRecvThread;
boost::thread* _multiSendThread;
boost::thread* _multiRecvThread;
std::unordered_map<std::string, DataBaseElement*> _dataBase;

//multicast receiving
boost::asio::ip::udp::socket* socket_recv;
boost::asio::ip::udp::endpoint sender_endpoint_;
enum { max_length = 1024 };
char multiCastData[max_length];
boost::asio::io_service io_service_recv;
const short multicast_port = 30001;

//multicast sending
boost::asio::io_service io_service_send;
boost::asio::ip::udp::endpoint* endpoint_;
boost::asio::ip::udp::socket* socket_send;
boost::asio::deadline_timer* timer_;
int message_count_;
std::string message_;

};
#endif
