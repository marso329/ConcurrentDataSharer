#include <concurrentdatasharer.h>
ConcurrentDataSharer::ConcurrentDataSharer(std::string const& groupName,
		std::string const& multicastadress, std::string const & listenadress,
		const short multicastport) :
		_groupName(groupName), multicast_address(
				boost::asio::ip::address::from_string(multicastadress)), listen_address(
				boost::asio::ip::address::from_string(listenadress)), multicast_port(
				multicastport), _name(generateRandomName(10)) {
	_multiSendQueue = new BlockingQueue<QueueElementBase*>(255);
	_recvQueue = new BlockingQueue<QueueElementBase*>(255);
	_TCPSendQueue = new BlockingQueue<QueueElementBase*>(255);

	//create my identity
	_myself = new clientData(_name, getLocalIPV4Adresses(),
			getLocalIPV6Adresses());
	_clients[_name] = _myself;

	//handling clients
	_clientLock = new std::mutex();
	newClientCallback = NULL;

	_mainThread = new boost::thread(
			boost::bind(&ConcurrentDataSharer::mainLoop, this));
	_TCPRecvThread = new boost::thread(
			boost::bind(&ConcurrentDataSharer::TCPRecv, this));
	_TCPSendThread = new boost::thread(
			boost::bind(&ConcurrentDataSharer::TCPSend, this));
	_multiSendThread = new boost::thread(
			boost::bind(&ConcurrentDataSharer::MultiSend, this));
	_multiRecvThread = new boost::thread(
			boost::bind(&ConcurrentDataSharer::MultiRecv, this));

	//multicast
	IntroduceMyselfToGroup();
}
ConcurrentDataSharer::~ConcurrentDataSharer() {

}

void ConcurrentDataSharer::IntroduceMyselfToGroup() {
	std::ostringstream ss;
	boost::archive::text_oarchive ar(ss);
	ar << _myself;
	std::string outbound_data = ss.str();
	QueueElementMultiSend* data = new QueueElementMultiSend(_name,
			outbound_data, MULTIINTRODUCTION);
	_multiSendQueue->Put(dynamic_cast<QueueElementBase*>(data));
}

std::vector<std::string> ConcurrentDataSharer::getClients() {
	std::vector<std::string> clients;
	_clientLock->lock();
	for (auto it = _clients.begin(); it != _clients.end(); it++) {
		clients.push_back(it->first);
	}
	_clientLock->unlock();
	return clients;
}

std::vector<std::string> ConcurrentDataSharer::getClientVariables(
		std::string const & client) {
	QueueElementTCPSend * toSend = new QueueElementTCPSend(client, "",
			TCPSENDVARIABLES, true);
	_TCPSendQueue->Put(dynamic_cast<QueueElementBase*>(toSend));
	std::cout << "put tcp package for client variables in queue" << std::endl;
	std::string data = toSend->getData();
	delete toSend;
	std::istringstream ar(data);
	boost::archive::text_iarchive ia(ar);
	std::vector<std::string> returnData;
	ia >> returnData;
	return returnData;
}

//runs from mainloop, keep minimal
void ConcurrentDataSharer::handleTCPRecv(QueueElementBase* data) {
	if (QueueElementSet* operation = dynamic_cast<QueueElementSet*>(data)) {
		if (_dataBase.find(operation->getName()) == _dataBase.end()) {
			//check all other peers if they have this name
			_dataBase[operation->getName()] = new DataBaseElement(operation);
		} else {
			_dataBase[operation->getName()]->setData(operation->getData());
			_dataBase[operation->getName()]->runCallback();
		}
		delete data;
	} else if (QueueElementCallback* operation =
			dynamic_cast<QueueElementCallback*>(data)) {
		if (_dataBase.find(operation->getName()) != _dataBase.end()) {
			//check all other peers if they have this name
			_dataBase[operation->getName()]->setCallback(
					operation->getCallback());
		}
		delete data;
	}

	else if (QueueElementGet* operation = dynamic_cast<QueueElementGet*>(data)) {
		if (_dataBase.find(operation->getName()) == _dataBase.end()) {
			//check all other peers if they have this name
			//_dataBase[operation->getName()]=new DataBaseElement(operation);
			operation->setData("");

		} else {
			DataBaseElement* element = _dataBase[operation->getName()];
			operation->setData(element->getData());
		}
	}
}

void ConcurrentDataSharer::handleQueueElementTCPSend(
		QueueElementTCPSend* data) {
	std::cout<<"handleQueueElementTCPSend handling data"<<std::endl;
	switch (data->getPurpose()) {
	case TCPSENDVARIABLES: {
		std::cout<<"TCPSENDVARIABLES"<<std::endl;
		std::vector<std::string> variables;
		for (auto it = _dataBase.begin(); it != _dataBase.end(); it++) {
			variables.push_back(it->first);
		}
		std::ostringstream ss;
		boost::archive::text_oarchive ar(ss);
		ar << variables;
		std::string outbound_data = ss.str();

		QueueElementTCPSend* toSend = new QueueElementTCPSend(
				data->getRequestor(), outbound_data, TCPREPLYVARIABLES, false);
		std::cout << "got request for variables from: " << data->getRequestor()
				<< std::endl;
		toSend->setTag(data->getTag());
		toSend->setRequestor(getMyName());
		_TCPSendQueue->Put(dynamic_cast<QueueElementBase*>(toSend));
		std::cout << "got request for tcp variables from" << data->getName()
				<< std::endl;
		delete data;
		break;
	}
	case TCPUNDEFINED: {
		throw std::runtime_error("undefined tcp action");
		break;
	}
	case TCPREPLYVARIABLES: {
		std::cout<<"TCPREPLYVARIABLES"<<std::endl;
		std::cout << "got variables" << std::endl;
		auto it = _requests.find(data->getTag());
		if (it == _requests.end()) {
			throw std::runtime_error("request not found");
		}
		it->second->setData(data->getDataNoneBlocking());
		_requests.erase(it);
		delete data;
		std::cout << "got reply ";
		break;
	}
	case TCPPERSONALINTRODUCTION: {
		std::cout<<"TCPPERSONALINTRODUCTION"<<std::endl;
		std::istringstream stream(data->getDataNoneBlocking());
		boost::archive::text_iarchive archive(stream);
		clientData* dataReceived = new clientData();
		archive >> dataReceived;
		std::cout << "Node: " << dataReceived->getName()
				<< " sent an personal introduction" << std::endl;

		std::cout << "he has ipadresses:" << std::endl;
		std::vector<std::string> adress = dataReceived->getIPV4();
		for (auto it = adress.begin(); it != adress.end(); it++) {
			std::cout << *it << std::endl;
		}
		bool change = false;
		_clientLock->lock();
		if (_clients.find(dataReceived->getName()) == _clients.end()) {
			_clients[dataReceived->getName()] = dataReceived;
			change = true;
		} else {
			delete dataReceived;
		}
		_clientLock->unlock();
		if (newClientCallback != NULL && change) {
			std::cout << "calling callack" << std::endl;
			newClientCallback();
		}

		break;
	}
	default: {
		throw std::runtime_error("unknown tcp action");
	}
	}

}

void ConcurrentDataSharer::handleMultiRecv(QueueElementBase* data) {
	QueueElementMultiSend* package = dynamic_cast<QueueElementMultiSend*>(data);
	switch (package->getPurpose()) {
	case MULTIUNDEFINED: {
		throw std::runtime_error("undefined multisend package");
		break;
	}
	case MULTIINTRODUCTION: {

		std::istringstream stream(package->getData());
		boost::archive::text_iarchive archive(stream);
		clientData* dataReceived = new clientData();
		archive >> dataReceived;

		if (dataReceived->getName() == getMyName()) {
			std::cout << "got multi introduction from myself" << std::endl;
			delete dataReceived;
			break;
		}
		std::cout << "Node: " << dataReceived->getName() << " joined"
				<< std::endl;
		std::cout << "he has ipadresses:" << std::endl;
		std::vector<std::string> adress = dataReceived->getIPV4();
		for (auto it = adress.begin(); it != adress.end(); it++) {
			std::cout << *it << std::endl;
		}
		bool change = false;
		_clientLock->lock();
		if (_clients.find(dataReceived->getName()) == _clients.end()) {
			_clients[dataReceived->getName()] = dataReceived;
			change = true;
		}
		_clientLock->unlock();
		if (newClientCallback != NULL && change) {
			std::cout << "calling callack" << std::endl;
			newClientCallback();
		}
		std::ostringstream ss;
		boost::archive::text_oarchive ar(ss);
		ar << _myself;
		std::string outbound_data = ss.str();
		QueueElementTCPSend* toSend = new QueueElementTCPSend(
				dataReceived->getName(), outbound_data, TCPPERSONALINTRODUCTION,
				false);
		std::cout << "sent personal introduction" << std::endl;
		_TCPSendQueue->Put(dynamic_cast<QueueElementBase*>(toSend));
		if (!change) {
			delete dataReceived;
		}

		break;
	}
	default: {
		throw std::runtime_error("shit is fucked");
		break;
	}

	};
	delete data;
}

void ConcurrentDataSharer::mainLoop() {
	while (true) {
		QueueElementBase* data = _recvQueue->Take();
		std::cout << "mainloop handling data" << std::endl;

		if (QueueElementSet* package = dynamic_cast<QueueElementSet*>(data)) {
			handleTCPRecv(data);
		} else if (QueueElementGet* package =
				dynamic_cast<QueueElementGet*>(data)) {
			handleTCPRecv(data);
		} else if (QueueElementCallback* package =
				dynamic_cast<QueueElementCallback*>(data)) {
			handleTCPRecv(data);
		} else if (QueueElementTCPSend* package =
				dynamic_cast<QueueElementTCPSend*>(data)) {
			std::cout << "mainloop sending to handleQueueElementTCPSend"
					<< std::endl;
			handleQueueElementTCPSend(package);
			//handleTCPRecv(data);
		}

		else if (QueueElementMultiSend* package =
				dynamic_cast<QueueElementMultiSend*>(data)) {
			handleMultiRecv(data);
		}

		else {
			throw std::runtime_error("no casting in mainloop succesfull");
		}
	}

}
void ConcurrentDataSharer::handleTCPRecvData(
		const boost::system::error_code& error, size_t bytes_recvd,
		boost::shared_ptr<boost::asio::ip::tcp::socket> sock) {
	if (error) {
		std::cout << "error" << std::endl;
		return;

	}

	std::size_t bufferSize = sock->available();
	std::cout << "buffersize" << bufferSize << std::endl;
	if (bufferSize == 0) {
		return;
	}
	char* buffer = new char[bufferSize];
	char* headerBuffer = new char[32];
	boost::system::error_code ec;
	unsigned int packetSize = sock->read_some(
			boost::asio::buffer(buffer, bufferSize), ec);

	if (ec) {
		throw std::runtime_error("fetch failed");
	}
	memcpy(headerBuffer, buffer, 32);
	std::istringstream is(std::string(headerBuffer, 32));
	std::size_t inbound_data_size = 0;
	if (!(is >> std::hex >> inbound_data_size)) {
		throw std::runtime_error("incorrect header");
	}

	std::istringstream data(std::string(buffer + 32, inbound_data_size));

	if (!error) {
		boost::archive::text_iarchive archive(data);
		QueueElementTCPSend* dataReceived = new QueueElementTCPSend();
		archive >> dataReceived;
		_recvQueue->Put(dynamic_cast<QueueElementBase*>(dataReceived));

	} else {
		throw std::runtime_error("multiRecv error");
	}
	delete buffer;
	delete headerBuffer;
	/**	sock->async_receive(boost::asio::null_buffers(),
	 boost::bind(&ConcurrentDataSharer::handleTCPRecvData, this,
	 boost::asio::placeholders::error,
	 boost::asio::placeholders::bytes_transferred, sock));
	 **/
}

void ConcurrentDataSharer::TCPRecvSession(
		boost::shared_ptr<boost::asio::ip::tcp::socket> sock) {
	std::cout << "TCPRecvSession" << std::endl;
	std::cout << "avail: " << sock->available() << std::endl;
	std::size_t bufferSize = sock->available();

	char* headerBuffer = new char[32];
	boost::system::error_code ec;
//	unsigned int packetSize = sock->read_some(
//			boost::asio::buffer(buffer, bufferSize), ec);
std::size_t dataRead =boost::asio::read(*sock, boost::asio::buffer(headerBuffer, 32));
if(dataRead!=32){
	std::cout<<"header read failed";
}

	std::cout << "reveived: " << dataRead << " bytes of data for header" << std::endl;
	if (ec) {
		throw std::runtime_error("fetch failed");
	}
	//memcpy(headerBuffer, buffer, 32);
	std::istringstream is(std::string(headerBuffer, 32));
	std::size_t inbound_data_size = 0;
	if (!(is >> std::hex >> inbound_data_size)) {
		throw std::runtime_error("incorrect header");
	}
	char* buffer = new char[inbound_data_size];

	std::cout << "giong to try to receive: " << inbound_data_size << " bytes of data" << std::endl;

	dataRead =boost::asio::read(*sock, boost::asio::buffer(buffer, inbound_data_size));
	std::cout << "reveived: " << dataRead << " bytes of data" << std::endl;


	std::istringstream data(std::string(buffer, inbound_data_size));

	boost::archive::text_iarchive archive(data);
	QueueElementTCPSend* dataReceived = new QueueElementTCPSend();
	archive >> dataReceived;
	std::cout << "put TCP data on recvQueue";
	_recvQueue->Put(dynamic_cast<QueueElementBase*>(dataReceived));

	delete buffer;
	delete headerBuffer;
}

void ConcurrentDataSharer::TCPRecv() {
	boost::asio::ip::tcp::acceptor acceptor(io_service_TCP_recv,
			boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(),
					TCP_recv_port));
	while (true) {
		boost::shared_ptr<boost::asio::ip::tcp::socket> sock(
				new boost::asio::ip::tcp::socket(io_service_TCP_recv));
		acceptor.accept(*sock);
		std::cout << "accepted on vall" << std::endl;
		boost::thread t(
				boost::bind(&ConcurrentDataSharer::TCPRecvSession, this, sock));
	}

}


void ConcurrentDataSharer::TCPSend() {
	//socket_TCP_send = new boost::asio::ip::tcp::socket(io_service_TCP_send);
	while (true) {
		{
			QueueElementBase* data = _TCPSendQueue->Take();
			boost::shared_ptr<boost::asio::ip::tcp::socket> socket_TCP_send(
					new boost::asio::ip::tcp::socket(io_service_TCP_recv));
			boost::shared_ptr<boost::asio::ip::tcp::resolver> resolver_TCP(
					new boost::asio::ip::tcp::resolver(io_service_TCP_send));

			std::cout << "sendt on package" << std::endl;
			QueueElementTCPSend* operation =
					dynamic_cast<QueueElementTCPSend*>(data);
			operation->setRequestor(getMyName());
			if (operation->getResponsRequired()) {
				operation->setTag(generateRandomName(20));
				while (_requests.find(operation->getTag()) != _requests.end()) {
					operation->setTag(generateRandomName(20));
				}
				_requests[operation->getTag()] = operation;

			}
			std::ostringstream port;
			port << TCP_recv_port;
			_clientLock->lock();
			auto it = _clients.find(operation->getName());
			if (it == _clients.end()) {
				std::cout << "could not find client" << std::endl;
				delete data;
				_clientLock->unlock();
				continue;
			}
			std::vector<std::string> clientadresses = (*it).second->getIPV4();
			_clientLock->unlock();
			if (clientadresses.size() == 0) {
				std::cout << "found client but no adress associated"
						<< std::endl;
				delete data;
				continue;
			}
			boost::asio::ip::tcp::resolver::iterator endpoint =
					resolver_TCP->resolve(
							boost::asio::ip::tcp::resolver::query(
									clientadresses[0], port.str()));
			boost::asio::connect(*socket_TCP_send, endpoint);

			//prepare data
			std::ostringstream ss;
			boost::archive::text_oarchive ar(ss);
			ar << operation;
			std::string outbound_data = ss.str();
			//prepare header
			std::ostringstream header_stream;
			header_stream << std::setw(32) << std::hex << outbound_data.size();
			std::string outbound_header = header_stream.str();
			//push header and data to buffer

			static char eol[] = { '\n' };

			boost::shared_ptr<std::vector<boost::asio::const_buffer>> buffers(new std::vector<boost::asio::const_buffer>);
			buffers->push_back(boost::asio::buffer(outbound_header));
			buffers->push_back(boost::asio::buffer(outbound_data));
			//buffers.push_back(boost::asio::buffer(eol));
			//socket_TCP_send->send(buffers);
			boost::system::error_code ec;

			std::size_t dataSent= boost::asio::write(*socket_TCP_send,*buffers,ec);
		std::cout<<"sent: "<<dataSent<<" bytes"<<std::endl;
		if(ec){
			std::cout<<"transmission errroer"<<std::endl;

		}
			//socket_TCP_send->w
			/**boost::asio::async_write(*socket_TCP_send, *buffers,
					boost::bind(&ConcurrentDataSharer::handleTCPSendError, this,
							boost::asio::placeholders::error,
							boost::asio::placeholders::bytes_transferred,buffers));
			**/
			io_service_send.run();
			std::cout << "io_service run once returned" << std::endl;
			if (!operation->getResponsRequired()) {
				delete data;
			}
		}
	}
}


void ConcurrentDataSharer::handleTCPSendError(
		const boost::system::error_code& error, size_t bytes_recvd) {
	if (error) {
		throw std::runtime_error("Multicast send failed");
	}

	std::cout << "successfully sent: " << bytes_recvd << " bytes of data"
			<< std::endl;

}

void ConcurrentDataSharer::handleMultiSendError(
		const boost::system::error_code& error, size_t bytes_recvd) {
	if (error) {
		throw std::runtime_error("Multicast send failed");
	}

}
void ConcurrentDataSharer::MultiSend() {
	//initialize socket
	endpoint_ = new boost::asio::ip::udp::endpoint(multicast_address,
			multicast_port);

	socket_send = new boost::asio::ip::udp::socket(io_service_send,
			endpoint_->protocol());
	while (true) {
		//get element from queue
		QueueElementBase* element = _multiSendQueue->Take();
		QueueElementMultiSend* sendElement =
				dynamic_cast<QueueElementMultiSend*>(element);
		if (sendElement == NULL) {
			throw std::runtime_error("casting in Multisend failed");
		}
		//prepare data
		std::ostringstream ss;
		boost::archive::text_oarchive ar(ss);
		ar << sendElement;
		std::string outbound_data = ss.str();
		//prepare header
		std::ostringstream header_stream;
		header_stream << std::setw(32) << std::hex << outbound_data.size();
		std::string outbound_header = header_stream.str();
		//push header and data to buffer
		std::vector<boost::asio::const_buffer> buffers;
		buffers.push_back(boost::asio::buffer(outbound_header));
		buffers.push_back(boost::asio::buffer(outbound_data));
		//send buffer
		socket_send->async_send_to(buffers, *endpoint_,
				boost::bind(&ConcurrentDataSharer::handleMultiSendError, this,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
		io_service_send.run();
		delete sendElement;
	}
}

void ConcurrentDataSharer::handleMultiRecvData(
		const boost::system::error_code& error, size_t bytes_recvd) {

	std::size_t bufferSize = socket_recv->available();
	char* buffer = new char[bufferSize];
	char* headerBuffer = new char[32];
	boost::system::error_code ec;
	unsigned int packetSize = socket_recv->receive_from(
			boost::asio::buffer(buffer, bufferSize), sender_endpoint_, 0, ec);

	if (ec) {
		throw std::runtime_error("fetch failed");
	}
	memcpy(headerBuffer, buffer, 32);
	std::istringstream is(std::string(headerBuffer, 32));
	std::size_t inbound_data_size = 0;
	if (!(is >> std::hex >> inbound_data_size)) {
		throw std::runtime_error("incorrect header");
	}

	std::istringstream data(std::string(buffer + 32, inbound_data_size));

	if (!error) {
		boost::archive::text_iarchive archive(data);
		QueueElementMultiSend* dataReceived = new QueueElementMultiSend();
		archive >> dataReceived;
		_recvQueue->Put(dynamic_cast<QueueElementBase*>(dataReceived));

	} else {
		throw std::runtime_error("multiRecv error");
	}
	delete buffer;
	delete headerBuffer;
	socket_recv->async_receive(boost::asio::null_buffers(),
			boost::bind(&ConcurrentDataSharer::handleMultiRecvData, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
}

void ConcurrentDataSharer::MultiRecv() {
	socket_recv = new boost::asio::ip::udp::socket(io_service_recv);
	boost::asio::ip::udp::endpoint listen_endpoint(listen_address,
			multicast_port);
	socket_recv->open(listen_endpoint.protocol());
	socket_recv->set_option(boost::asio::ip::udp::socket::reuse_address(true));
	socket_recv->bind(listen_endpoint);
	socket_recv->set_option(
			boost::asio::ip::multicast::join_group(multicast_address));
	socket_recv->set_option(boost::asio::ip::multicast::enable_loopback(true));

	socket_recv->async_receive(boost::asio::null_buffers(),
			boost::bind(&ConcurrentDataSharer::handleMultiRecvData, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
	io_service_recv.run();
}

std::string ConcurrentDataSharer::generateRandomName(std::size_t size) {
	const char alphanum[] = "0123456789"
			"!@#$%^&*"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz";

	int stringLength = sizeof(alphanum) - 1;
	srand(time(NULL));
	std::string name;
	for (std::size_t i = 0; i < size; i++) {
		name += alphanum[rand() % stringLength];
	}
	return name;

}
std::vector<std::string> ConcurrentDataSharer::getLocalIPV4Adresses() {
	std::vector<std::string> adresses;
	struct ifaddrs * ifAddrStruct = NULL;
	struct ifaddrs * ifa = NULL;
	void * tmpAddrPtr = NULL;

	getifaddrs(&ifAddrStruct);

	for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
		if (!ifa->ifa_addr) {
			continue;
		}
		if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
			// is a valid IP4 Address
			tmpAddrPtr = &((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
			char addressBuffer[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
			if (strcmp(addressBuffer, "127.0.0.1") != 0) {
				adresses.push_back(std::string(addressBuffer));
			}
		}
	}
	if (ifAddrStruct != NULL)
		freeifaddrs(ifAddrStruct);
	return adresses;;
}
std::vector<std::string> ConcurrentDataSharer::getLocalIPV6Adresses() {
	std::vector<std::string> adresses;
	struct ifaddrs * ifAddrStruct = NULL;
	struct ifaddrs * ifa = NULL;
	void * tmpAddrPtr = NULL;

	getifaddrs(&ifAddrStruct);

	for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
		if (!ifa->ifa_addr) {
			continue;
		}
		if (ifa->ifa_addr->sa_family == AF_INET6) { // check it is IP6
			// is a valid IP6 Address
			tmpAddrPtr = &((struct sockaddr_in6 *) ifa->ifa_addr)->sin6_addr;
			char addressBuffer[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
			adresses.push_back(std::string(addressBuffer));
		}
	}
	if (ifAddrStruct != NULL)
		freeifaddrs(ifAddrStruct);
	return adresses;;
}
