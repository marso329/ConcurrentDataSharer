#ifndef CONCURRENTDATASHARER_H_
#define CONCURRENTDATASHARER_H_
#include <string>
#include <sstream>
#include <cstddef> // NULL
#include <iomanip>
#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <mutex>

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

class ConcurrentDataSharer {
public:
	ConcurrentDataSharer(std::string const & groupname,
			std::string const & multicastadress = "239.255.0.1",
			std::string const & listenadress = "0.0.0.0",
			const short multicastport = 30001);
	~ConcurrentDataSharer();
	template<typename T>
	void set(std::string const& name, T data) {
		std::ostringstream ss;
		boost::archive::text_oarchive ar(ss);
		ar << data;
		QueueElementSet* element = new QueueElementSet(name, ss.str());
		_recvQueue->Put(element);

	}

	template<typename T>
	T get(std::string const& name) {
		QueueElementGet* element = new QueueElementGet(name);
		_recvQueue->Put(element);
		std::string data = element->getData();
		delete element;
		std::istringstream ar(data);
		boost::archive::text_iarchive ia(ar);
		T returnData;
		ia >> returnData;
		return returnData;
	}
	std::vector<std::string> getClients();
	void registerNewClientCallback(CallbackSig func){
		newClientCallback=func;
	}
	void registerCallback(std::string const& name,CallbackSig func){
		QueueElementCallback* element = new QueueElementCallback(name,func);
		_recvQueue->Put(element);
	}

protected:
private:
	ConcurrentDataSharer();
	std::string generateRandomName(std::size_t);
	std::vector<std::string> getLocalIPV4Adresses();
	std::vector<std::string> getLocalIPV6Adresses();
	//thread functions
	void mainLoop();
	void TCPRecv();
	void TCPSend();
	void MultiSend();
	void MultiRecv();

	//group functions
	void IntroduceMyselfToGroup();

	//handle functions
	void handleTCPRecv(QueueElementBase*);
	void TCPRecvSession(boost::shared_ptr<boost::asio::ip::tcp::socket>  sock);
	void handleTCPRecvData(const boost::system::error_code& error,
			size_t bytes_recvd,boost::shared_ptr<boost::asio::ip::tcp::socket> sock);


	void handleMultiRecv(QueueElementBase*);

	void handleMultiRecvData(const boost::system::error_code& error,
			size_t bytes_recvd);
	void handleMultiSendError(const boost::system::error_code& error, size_t);

//const and initialized at creation

//name of the group
	const std::string _groupName;
	//multicast
	const boost::asio::ip::address multicast_address;
	const boost::asio::ip::address listen_address;
	const short multicast_port;
	const std::string _name;

	BlockingQueue<QueueElementBase*>* _multiSendQueue;
	BlockingQueue<QueueElementBase*>* _recvQueue;
	BlockingQueue<QueueElementBase*>* _TCPSendQueue;
	boost::thread* _mainThread;
	boost::thread* _TCPSendThread;
	boost::thread* _TCPRecvThread;
	boost::thread* _multiSendThread;
	boost::thread* _multiRecvThread;
	std::unordered_map<std::string, DataBaseElement*> _dataBase;

//multicast receiving
	boost::asio::ip::udp::socket* socket_recv;
	boost::asio::ip::udp::endpoint sender_endpoint_;
	const std::size_t header_size_ = 32;
	char inbound_header_[32];
	std::vector<char> inbound_data_;
	boost::asio::io_service io_service_recv;

//multicast sending
	boost::asio::io_service io_service_send;
	boost::asio::ip::udp::endpoint* endpoint_;
	boost::asio::ip::udp::socket* socket_send;

//TCP recv
	 boost::asio::io_service io_service_TCP_recv;
	 const short TCP_recv_port=30010;

	//client handling
	std::unordered_map<std::string, clientData*> _clients;
	clientData* _myself;
	std::mutex* _clientLock;
	CallbackSig newClientCallback;

};
#endif
