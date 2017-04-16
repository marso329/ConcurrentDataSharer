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
			outbound_data, INTRODUCTION);
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
	else if(QueueElementTCPRecv* operation = dynamic_cast<QueueElementTCPRecv*>(data)){
		std::cout<<"received tcp package"<<std::endl;
	}
}
void ConcurrentDataSharer::handleMultiRecv(QueueElementBase* data) {
	QueueElementMultiSend* package = dynamic_cast<QueueElementMultiSend*>(data);
	switch (package->getPurpose()) {
	case UNDEFINED: {
		throw std::runtime_error("undefined multisend package");
		break;
	}
	case INTRODUCTION: {
		std::cout << "Node: " << package->getName() << " joined" << std::endl;

		std::istringstream data(package->getData());
		boost::archive::text_iarchive archive(data);
		clientData* dataReceived = new clientData();
		archive >> dataReceived;
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
		throw std::runtime_error("shit is fucked");
		break;
	}

	};
	delete data;
}

void ConcurrentDataSharer::mainLoop() {
	while (true) {
		QueueElementBase* data = _recvQueue->Take();

		if (QueueElementSet* package = dynamic_cast<QueueElementSet*>(data)) {
			handleTCPRecv(data);
		} else if (QueueElementGet* package =
				dynamic_cast<QueueElementGet*>(data)) {
			handleTCPRecv(data);
		} else if (QueueElementCallback* package =
				dynamic_cast<QueueElementCallback*>(data)) {
			handleTCPRecv(data);
		} else if (QueueElementTCPRecv* package =
				dynamic_cast<QueueElementTCPRecv*>(data)) {
			handleTCPRecv(data);
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

	std::size_t bufferSize = sock->available();
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
		QueueElementTCPRecv* dataReceived = new QueueElementTCPRecv();
		archive >> dataReceived;
		_recvQueue->Put(dynamic_cast<QueueElementBase*>(dataReceived));

	} else {
		throw std::runtime_error("multiRecv error");
	}
	delete buffer;
	delete headerBuffer;
	sock->async_receive(boost::asio::null_buffers(),
			boost::bind(&ConcurrentDataSharer::handleTCPRecvData, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred, sock));

}

void ConcurrentDataSharer::TCPRecvSession(
		boost::shared_ptr<boost::asio::ip::tcp::socket> sock) {

	sock->async_receive(boost::asio::null_buffers(),
			boost::bind(&ConcurrentDataSharer::handleTCPRecvData, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred, sock));
	io_service_TCP_recv.run();
}

void ConcurrentDataSharer::TCPRecv() {
	boost::asio::ip::tcp::acceptor acceptor(io_service_TCP_recv,
			boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(),
					TCP_recv_port));
	while (true) {
		boost::shared_ptr<boost::asio::ip::tcp::socket> sock(
				new boost::asio::ip::tcp::socket(io_service_TCP_recv));
		acceptor.accept(*sock);
		boost::thread t(
				boost::bind(&ConcurrentDataSharer::TCPRecvSession, this, sock));
	}

}

void ConcurrentDataSharer::TCPSend() {

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
			adresses.push_back(std::string(addressBuffer));

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
