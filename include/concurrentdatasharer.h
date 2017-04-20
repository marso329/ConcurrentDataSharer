#ifndef CONCURRENTDATASHARER_H_
#define CONCURRENTDATASHARER_H_

//standard includes
#include <string>
#include <sstream>
#include <cstddef>
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
#include <unordered_map>

//boost includes
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
#include <boost/exception/all.hpp>

//own includes
#include "structures.h"
#include "BlockingQueue.h"


typedef boost::error_info<struct tag_errmsg, std::string> errmsg_info;
/**
 * \class ConcurrentDataSharer
 *
 *
 * \brief 	ConcurrentDataSharer* sharer = new ConcurrentDataSharer("test");
 *	sharer->set<int>("data",43);
 *	sharer->set<int>("data1",42);
 *
 *This is the main class for the ConcurrentDataSharer
 *
 * \note This is the shit
 *
 * \author  Martin Soderen
 *
 * \version 0.1
 *
 * \date  2017/04/147 00:00:00
 *
 * Contact: martin.soderen@gmail.com
 *
 * Created on: Fri Apr 14 00:00:00 2017
 *
 * $Id: $
 *
 */
class ConcurrentDataSharer {
public:
	/** \brief Constructor
	 * \param groupname the name for the ConcurrentDatasharer group to share data with
	 * \param 	multicastadress the adress for UDP broadcasting, default is 239.255.0.1
	 * \param 	listenadress the adress for UDP broadcasting listening, default is 0.0.0.0
	 * \param multicastport the port for UDP broadcasting, default is 30001
	 */
	ConcurrentDataSharer(std::string const & groupname,
			std::string const & multicastadress = "239.255.0.1",
			std::string const & listenadress = "0.0.0.0",
			const short multicastport = 30001);

	/** \brief Destructor
	 *
	 * Destructs objects
	 */
	~ConcurrentDataSharer();

	/** \brief Set a local variable, creates it if it does not exists
	 * \param name the name of the variable
	 * \param 	data the data
	 */
	template<typename T>
	void set(std::string const& name, T data) {
		std::ostringstream ss;
		boost::archive::text_oarchive ar(ss);
		ar << data;
		QueueElementSet* element = new QueueElementSet(name, ss.str());
		_recvQueue->Put(element);

	}
	/** \brief Get a local variable
	 * \param name the name of the variable
	 */
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
	/** \brief Get a variable from another client
	 * \param client the name of the client
	 *\param name the name of the variable
	 */
	template<typename T>
	T get(std::string const& client, std::string const& name) {
		QueueElementTCPSend * toSend = new QueueElementTCPSend(client, name,
				TCPGETVARIABLE, true);
		_TCPSendQueue->Put(dynamic_cast<QueueElementBase*>(toSend));
		std::string data = toSend->getData();
		delete toSend;
		std::istringstream ar(data);
		boost::archive::text_iarchive ia(ar);
		T returnData;
		ia >> returnData;
		return returnData;
	}

	/** \brief to get all connected clients
	 * \return std::vector<std::string> with the names of all the clients
	 */
	std::vector<std::string> getClients();

	/** \brief connects a callback when a new clients connects
	 * \param  func the function to connect
	 */
	void registerNewClientCallback(CallbackSig func) {
		newClientCallback = func;
	}

	/** \brief connects a callback if a local variable changes
	 * \param name the name of the variable
	 * \param func the function to connect with change
	 */
	void registerCallback(std::string const& name, CallbackSig func) {
		QueueElementCallback* element = new QueueElementCallback(name, func);
		_recvQueue->Put(element);
	}
	/** \brief return the name of this client
	 * \return the name of this client
	 */
	std::string getMyName() {
		return _name;
	}
	/** \brief get a list with the names of a clients variables
	 * \param client the name of the client
	 * \return  a list with the names of a clients variables
	 */
	std::vector<std::string> getClientVariables(std::string const & client);

protected:
private:
	/** \brief deleted default constructor
	 */
	ConcurrentDataSharer();
	/** \brief generate a random string
	 * \param len the length of the random string
	 * \return  a random string
	 */
	std::string generateRandomName(std::size_t len);

	/** \brief returns a list with this clients IPV4 adresses
	 * \return  a list with this clients IPV4 adresses
	 */
	std::vector<std::string> getLocalIPV4Adresses();
	/** \brief returns a list with this clients IPV6 adresses
	 * \return  a list with this clients IPV6 adresses
	 */
	std::vector<std::string> getLocalIPV6Adresses();

	//thread functions

	/** \brief target function for the main thread _mainThread which access the _dataBase
	 * \see ConcurrentDataSharer::_mainThread
	 * \see ConcurrentDataSharer::_recvQueue
	 * \see ConcurrentDataSharer::_dataBase
	 * \see ConcurrentDataSharer::_dataBase
	 * \see ConcurrentDataSharer::handleTCPRecv(QueueElementBase*)
	 *\see ConcurrentDataSharer::handleQueueElementTCPSend(QueueElementTCPSend*);
	 * It takes QueueElementBase from _recvQueue and decides which function to use using dynamic_cast
	 */
	void mainLoop();

	/** \brief target function for the TCP thread _TCPRecvThread which accepts TCP connections and creates a TCP handler with TCPRecvSession as target function
	 * \see ConcurrentDataSharer::TCPRecvSession()
	 * \see ConcurrentDataSharer::_TCPRecvThread
	 */

	void TCPRecv();
	/** \brief target function for the TCP thread _TCPSendThread which takes QueueElementBase from _TCPSendQueue and creates a connection and sends the package to the endpoint
	 * \see ConcurrentDataSharer::TCPRecvSession()
	 * \see ConcurrentDataSharer::_TCPSendThread
	 */
	void TCPSend();

	/** \brief target function for the UDP thread _multiSendThread which takes QueueElementBase from _multiSendQueue and broadcast the message
	 * \see ConcurrentDataSharer::handleMultiSendError()
	 * \see ConcurrentDataSharer::_multiSendThread
	 */
	void MultiSend();
	/** \brief target function for the UDP thread _multiRecvThread which listens for UDP packages and puts them in the _recvQueue queue
	 * \see ConcurrentDataSharer::_multiRecvThread
	 */
	void MultiRecv();

	/** \brief sends out an broadcast over the LAN introducing this clients
	 */
	void IntroduceMyselfToGroup();

	//handle functions

	/** \brief used in the mainloop to handle TCP packages
	 * \see ConcurrentDataSharer::_mainThread
	 * \see ConcurrentDataSharer::mainLoop()
	 */
	void handleTCPRecv(QueueElementBase*);

	/** \brief Target function for threads created in TCPRecv which reads data from socket
	 * \see ConcurrentDataSharer::TCPRecv
	 */
	void TCPRecvSession(boost::shared_ptr<boost::asio::ip::tcp::socket> sock);

	/** \brief used in mainLoop to handle TCP packages
	 * \see ConcurrentDataSharer::mainLoop()
	 */
	void handleQueueElementTCPSend(QueueElementTCPSend*);

	/** \brief used in mainLoop to handle UDP packages
	 * \see ConcurrentDataSharer::mainLoop()
	 */
	void handleMultiRecv(QueueElementBase*);

	/** \brief handler used in MultiRecv for receiving UDP data
	 * \see ConcurrentDataSharer::MultiRecv()
	 */
	void handleMultiRecvData(const boost::system::error_code& error,
			size_t bytes_recvd);

	/** \brief error handlar in MultiSend
	 * \see ConcurrentDataSharer::MultiSend()
	 */
	void handleMultiSendError(const boost::system::error_code& error, size_t);

//const and initialized at creation

	///name of the group
	const std::string _groupName;
	//multicast
	///the multicast adress
	const boost::asio::ip::address multicast_address;
	///the multicast listening adress
	const boost::asio::ip::address listen_address;
	///the multicast port
	const short multicast_port;
	///this clients name
	const std::string _name;
	/// queue used to send QueueElementMultiSend elements to _multiSendThread
	BlockingQueue<QueueElementBase*>* _multiSendQueue;
	///queue used by _TCPRecvThread and _multiRecvThread to send QueueElementBase to _mainThread
	BlockingQueue<QueueElementBase*>* _recvQueue;
	/// queue used to send QueueElementTCPSend elements to _TCPSendThread
	BlockingQueue<QueueElementBase*>* _TCPSendQueue;
	///threads that handles data from _TCPRecvThread and _multiRecvThread
	boost::thread* _mainThread;
	///thread which is used to send TCP packages
	boost::thread* _TCPSendThread;
	///thread used to receive TCP packages
	boost::thread* _TCPRecvThread;
	///thread used to send UDP packages
	boost::thread* _multiSendThread;
	///thread used to receive UDP packages
	boost::thread* _multiRecvThread;
	///the database storing local variables
	std::unordered_map<std::string, DataBaseElement*> _dataBase;

//multicast receiving
	///socket used for receiving UDP packages
	boost::asio::ip::udp::socket* socket_recv;
	///UDP receiving endpoint
	boost::asio::ip::udp::endpoint sender_endpoint_;
	///header size for both UDP and TCP packages
	const std::size_t header_size_ = 32;
	///UDP io_service
	boost::asio::io_service io_service_recv;

//multicast sending
	///UDP sending io_service
	boost::asio::io_service io_service_send;
	///UDP sending endpoint
	boost::asio::ip::udp::endpoint* endpoint_;
	///UDP sending socket
	boost::asio::ip::udp::socket* socket_send;

//TCP recv
	///TCP sending io_service
	boost::asio::io_service io_service_TCP_recv;
	///TCP sending port, should be set in constructor
	short unsigned TCP_recv_port = 30010;

//TCP send
	///TCP sending io_service
	boost::asio::io_service io_service_TCP_send;
	///used for allowing the TCP thread to find good port before introductions
	std::mutex* _TCPLock;
	bool TCP_port_chosen;
	std::condition_variable TCP_chosen_cond;

//request connection
	///map for request send to other clients
	std::unordered_map<std::string, QueueElementTCPSend*> _requests;

	//client handling
	///map for storing clients
	std::unordered_map<std::string, clientData*> _clients;
	///this clients clientData
	clientData* _myself;
	///lock for locking down _clients since it can be access from user side
	std::mutex* _clientLock;
	///function pointer for new client callback
	CallbackSig newClientCallback;

};
#endif
