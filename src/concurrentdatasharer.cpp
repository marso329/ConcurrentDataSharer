#include <concurrentdatasharer.h>
ConcurrentDataSharer::ConcurrentDataSharer(std::string const& groupName,
		std::string const& multicastadress, std::string const & listenadress,
		const short multicastport) :
		_groupName(groupName), multicast_address(
				boost::asio::ip::address::from_string(multicastadress)), listen_address(
				boost::asio::ip::address::from_string(listenadress)), multicast_port(
				multicastport), _name(generateRandomName(10)) {
	logging_buffer = new logger();
	logging_buffer->push_back("Created instance");
	_multiSendQueue = new BlockingQueue<QueueElementBase*>(255);
	_recvQueue = new BlockingQueue<QueueElementBase*>(255);
	_TCPSendQueue = new BlockingQueue<QueueElementBase*>(255);

	//handling clients
	_clientLock = new std::mutex();
	newClientCallback = NULL;

	_TCPLock = new std::mutex();
	TCP_port_chosen = false;

	//let it find a good port
	_TCPRecvThread = new boost::thread(
			boost::bind(&ConcurrentDataSharer::TCPRecv, this));
	{
		std::unique_lock < std::mutex > lock(*_TCPLock);
		while (!TCP_port_chosen) {
			TCP_chosen_cond.wait(lock);
		}
	}
	{
		std::ostringstream temp;
		temp << "TCP port " << TCP_recv_port << " chosen";
		logging_buffer->push_back(temp.str());

	}

	//create my identity
	_myself = new clientData(_name, getLocalIPV4Adresses(),
			getLocalIPV6Adresses(), TCP_recv_port);

	{
		std::ostringstream temp;
		temp << "Name " << _name << " chosen";
		logging_buffer->push_back(temp.str());

	}
	_clients[_name] = _myself;

	_mainThread = new boost::thread(
			boost::bind(&ConcurrentDataSharer::mainLoop, this));
	_TCPSendThread = new boost::thread(
			boost::bind(&ConcurrentDataSharer::TCPSend, this));
	_multiSendThread = new boost::thread(
			boost::bind(&ConcurrentDataSharer::MultiSend, this));
	_multiRecvThread = new boost::thread(
			boost::bind(&ConcurrentDataSharer::MultiRecv, this));

	_pingThread = new boost::thread(
			boost::bind(&ConcurrentDataSharer::pinger, this));
	//multicast
	IntroduceMyselfToGroup();
}
ConcurrentDataSharer::~ConcurrentDataSharer() {

}

void ConcurrentDataSharer::IntroduceMyselfToGroup() {
	{
		std::ostringstream temp;
		temp << "IntroduceMyselfToGroup";
		logging_buffer->push_back(temp.str());
	}
	std::ostringstream ss;
	boost::archive::text_oarchive ar(ss);
	ar << _myself;
	std::string outbound_data = ss.str();
	QueueElementMultiSend* data = new QueueElementMultiSend(_name,
			outbound_data, MULTIINTRODUCTION);
	_multiSendQueue->Put(dynamic_cast<QueueElementBase*>(data));
}

std::vector<std::string> ConcurrentDataSharer::getClients() {
	{
		std::ostringstream temp;
		temp << "getClients";
		logging_buffer->push_back(temp.str());
	}
	std::vector<std::string> clients;
	_clientLock->lock();
	for (auto it = _clients.begin(); it != _clients.end(); it++) {
		clients.push_back(it->first);
	}
	_clientLock->unlock();
	return clients;
}

std::vector<std::string> ConcurrentDataSharer::getLogger() {
	return logging_buffer->get_logs();
}

std::vector<std::string> ConcurrentDataSharer::getClientVariables(
		std::string const & client) {
	{
		std::ostringstream temp;
		temp << "getClientVariables" << " client: " << client;
		logging_buffer->push_back(temp.str());
	}
	QueueElementTCPSend * toSend = new QueueElementTCPSend(client, "",
			TCPSENDVARIABLES, true);
	_TCPSendQueue->Put(dynamic_cast<QueueElementBase*>(toSend));
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
	{
		std::ostringstream temp;
		temp << "handleTCPRecv";
		logging_buffer->push_back(temp.str());
	}
	if (QueueElementSet* operation = dynamic_cast<QueueElementSet*>(data)) {
		{
			std::ostringstream temp;
			temp << "handleTCPRecv QueueElementSet";
			logging_buffer->push_back(temp.str());
		}
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
		{
			std::ostringstream temp;
			temp << "handleTCPRecv QueueElementCallback";
			logging_buffer->push_back(temp.str());
		}
		if (_dataBase.find(operation->getName()) != _dataBase.end()) {
			//check all other peers if they have this name
			_dataBase[operation->getName()]->setCallback(
					operation->getCallback());
		}
		delete data;
	}

	else if (QueueElementGet* operation = dynamic_cast<QueueElementGet*>(data)) {
		{
			std::ostringstream temp;
			temp << "handleTCPRecv QueueElementGet";
			logging_buffer->push_back(temp.str());
		}
		if (_dataBase.find(operation->getName()) == _dataBase.end()) {
			//check all other peers if they have this name
			operation->setData("");

		} else {
			DataBaseElement* element = _dataBase[operation->getName()];
			operation->setData(element->getData());
		}
	}
	else if(QueueElementSubscribe* operation=dynamic_cast<QueueElementSubscribe*>(data)){
		QueueElementTCPSend* toSend = new QueueElementTCPSend(
					operation->getName(), operation->getVar(), TCPSTARTSUBSCRIPTION, false);
			_TCPSendQueue->Put(dynamic_cast<QueueElementBase*>(toSend));
			delete data;
	}
}

void ConcurrentDataSharer::handleQueueElementTCPSend(
		QueueElementTCPSend* data) {
	{
		std::ostringstream temp;
		temp << "handleQueueElementTCPSend";
		logging_buffer->push_back(temp.str());
	}
	switch (data->getPurpose()) {
	case TCPSENDVARIABLES: {
		{
			std::ostringstream temp;
			temp << "handleQueueElementTCPSend TCPSENDVARIABLES";
			logging_buffer->push_back(temp.str());
		}
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
		toSend->setTag(data->getTag());
		toSend->setRequestor(getMyName());
		_TCPSendQueue->Put(dynamic_cast<QueueElementBase*>(toSend));
		delete data;
		break;
	}
	case TCPUNDEFINED: {

		throw std::runtime_error("undefined tcp action");
		break;
	}
	case TCPREPLYVARIABLES: {
		{
			std::ostringstream temp;
			temp << "handleQueueElementTCPSend TCPREPLYVARIABLES";
			logging_buffer->push_back(temp.str());
		}
		auto it = _requests.find(data->getTag());
		if (it == _requests.end()) {
			throw std::runtime_error("request not found");
		}
		it->second->setData(data->getDataNoneBlocking());
		_requests.erase(it);
		delete data;
		break;
	}
	case TCPPERSONALINTRODUCTION: {
		{
			std::ostringstream temp;
			temp << "handleQueueElementTCPSend TCPPERSONALINTRODUCTION";
			logging_buffer->push_back(temp.str());
		}
		std::istringstream stream(data->getDataNoneBlocking());
		boost::archive::text_iarchive archive(stream);
		clientData* dataReceived = new clientData();
		archive >> dataReceived;
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
			newClientCallback();
		}
		QueueElementTCPSend* toSend = new QueueElementTCPSend(
				data->getRequestor(), "", TCPSENDCLIENTS, false);
		_TCPSendQueue->Put(toSend);
		delete data;
		break;
	}
	case TCPGETVARIABLE: {
		{
			std::ostringstream temp;
			temp << "handleQueueElementTCPSend TCPGETVARIABLE";
			logging_buffer->push_back(temp.str());
		}
		auto it = _dataBase.find(data->getDataNoneBlocking());
		std::string datafromDatabase = "";
		if (it != _dataBase.end()) {
			datafromDatabase = it->second->getData();
		}

		QueueElementTCPSend* toSend = new QueueElementTCPSend(
				data->getRequestor(), datafromDatabase, TCPREPLYGETVARIABLE,
				false);
		toSend->setTag(data->getTag());
		_TCPSendQueue->Put(toSend);
		delete data;
		break;
	}

	case TCPREPLYGETVARIABLE: {
		{
			std::ostringstream temp;
			temp << "handleQueueElementTCPSend TCPREPLYGETVARIABLE";
			logging_buffer->push_back(temp.str());
		}
		auto it = _requests.find(data->getTag());
		if (it == _requests.end()) {
			throw std::runtime_error("request not found");
		}
		it->second->setData(data->getDataNoneBlocking());
		_requests.erase(it);
		delete data;
		break;
	}
	case TCPSENDCLIENTS: {
		{
			std::ostringstream temp;
			temp << "handleQueueElementTCPSend TCPSENDCLIENTS";
			logging_buffer->push_back(temp.str());
		}
		std::ostringstream ss;
		boost::archive::text_oarchive ar(ss);
		_clientLock->lock();
		std::size_t tempSize = _clients.size();
		ar << tempSize;
		for (auto it = _clients.begin(); it != _clients.end(); it++) {
			ar << it->second;
		}
		_clientLock->unlock();
		std::string outbound_data = ss.str();
		QueueElementTCPSend* toSend = new QueueElementTCPSend(
				data->getRequestor(), outbound_data, TCPREPLYCLIENTS, false);
		_TCPSendQueue->Put(toSend);
		delete data;
		break;
	}
	case TCPREPLYCLIENTS: {
		{
			std::ostringstream temp;
			temp << "handleQueueElementTCPSend TCPREPLYCLIENTS";
			logging_buffer->push_back(temp.str());
		}
		std::istringstream stream(data->getDataNoneBlocking());
		boost::archive::text_iarchive archive(stream);
		std::size_t tempSize;
		std::vector<clientData*> tempData;
		archive >> tempSize;
		//clientData* dataReceived = new clientData();
		//archive >> dataReceived;
		for (std::size_t i = 0; i < tempSize; i++) {
			clientData* tempClientdata = new clientData();
			archive >> tempClientdata;
			tempData.push_back(tempClientdata);
		}
		_clientLock->lock();
		for (auto it = tempData.begin(); it != tempData.end(); it++) {
			if (_clients.find((*it)->getName()) == _clients.end()) {
				_clients[(*it)->getName()] = *it;
			} else {
				delete *it;
			}
		}
		_clientLock->unlock();
		delete data;
		break;
	}
	case TCPPING:{
		QueueElementTCPSend* toSend = new QueueElementTCPSend(
				data->getRequestor(), "", TCPPINGREPLY, false);
		_TCPSendQueue->Put(toSend);
		break;
	}
	case TCPPINGREPLY:{
		_clientLock->lock();
		auto it=_clients.find(data->getRequestor()) ;
		if (it != _clients.end()) {
			it->second->last_ping=std::chrono::high_resolution_clock::now();
			it->second->ping_failure_counter=0;
		}
		_clientLock->unlock();
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
		{
			std::ostringstream temp;
			temp << "handleMultiRecv MULTIINTRODUCTION";
			logging_buffer->push_back(temp.str());
		}
		std::istringstream stream(package->getData());
		boost::archive::text_iarchive archive(stream);
		clientData* dataReceived = new clientData();
		archive >> dataReceived;

		if (dataReceived->getName() == getMyName()) {
			delete dataReceived;
			break;
		}
		bool change = false;
		_clientLock->lock();
		if (_clients.find(dataReceived->getName()) == _clients.end()) {
			_clients[dataReceived->getName()] = dataReceived;
			change = true;
		}
		_clientLock->unlock();
		if (newClientCallback != NULL && change) {
			newClientCallback();
		}
		std::ostringstream ss;
		boost::archive::text_oarchive ar(ss);
		ar << _myself;
		std::string outbound_data = ss.str();
		QueueElementTCPSend* toSend = new QueueElementTCPSend(
				dataReceived->getName(), outbound_data, TCPPERSONALINTRODUCTION,
				false);
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
	logging_buffer->push_back("Main thread started");
	while (true) {
		QueueElementBase* data = _recvQueue->Take();
		logging_buffer->push_back("Main thread working on data");

		if (QueueElementSet* package = dynamic_cast<QueueElementSet*>(data)) {
			logging_buffer->push_back("Main thread working on QueueElementSet");
			handleTCPRecv(data);
		} else if (QueueElementGet* package =
				dynamic_cast<QueueElementGet*>(data)) {
			logging_buffer->push_back("Main thread working QueueElementGet");
			handleTCPRecv(data);
		} else if (QueueElementCallback* package =
				dynamic_cast<QueueElementCallback*>(data)) {
			logging_buffer->push_back(
					"Main thread working on QueueElementCallback");
			handleTCPRecv(data);
		} else if (QueueElementTCPSend* package =
				dynamic_cast<QueueElementTCPSend*>(data)) {
			logging_buffer->push_back(
					"Main thread working on QueueElementTCPSend");
			handleQueueElementTCPSend(package);
		} else if (QueueElementMultiSend* package =
				dynamic_cast<QueueElementMultiSend*>(data)) {
			logging_buffer->push_back(
					"Main thread working on QueueElementMultiSend");
			handleMultiRecv(data);
		}
		else if (QueueElementSubscribe* package =
						dynamic_cast<QueueElementSubscribe*>(data)) {
					logging_buffer->push_back(
							"Main thread working on QueueElementSubscribe");
					handleTCPRecv(data);
				}

		else {
			throw std::runtime_error("no casting in mainloop succesfull");
		}
	}

}

void ConcurrentDataSharer::pinger() {
	logging_buffer->push_back("Ping thread started");
	while (true) {
		_clientLock->lock();
		for (auto it = _clients.begin(); it != _clients.end();) {
			if(it->first==getMyName()){
				it++;
				continue;
			}
			QueueElementTCPSend* toSend = new QueueElementTCPSend(it->first, "",
					TCPPING, false);
			_TCPSendQueue->Put(dynamic_cast<QueueElementBase*>(toSend));
			auto time_now = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> diff = time_now
					- it->second->last_ping;
			if (diff > std::chrono::milliseconds(PINGTIMEFAILURE)) {
				it->second->ping_failure_counter += 1;
			}
			if(it->second->ping_failure_counter >NRPINGS){
				{
					std::ostringstream temp;
					temp<<"Deleting client "<<it->first<<" because it failed to respond to pings";
					logging_buffer->push_back(temp.str());
				}
				it=_clients.erase(it);
				continue;

			}
			it++;
		}
		_clientLock->unlock();
		boost::this_thread::sleep(boost::posix_time::milliseconds(PINGTIME));
	}

}

void ConcurrentDataSharer::TCPRecvSession(
		boost::shared_ptr<boost::asio::ip::tcp::socket> sock) {
	std::size_t bufferSize = sock->available();

	char* headerBuffer = new char[32];
	boost::system::error_code ec;
	std::size_t dataRead = boost::asio::read(*sock,
			boost::asio::buffer(headerBuffer, 32));

	if (ec) {
		throw std::runtime_error("fetch failed");
	}
	std::istringstream is(std::string(headerBuffer, 32));
	std::size_t inbound_data_size = 0;
	if (!(is >> std::hex >> inbound_data_size)) {
		throw std::runtime_error("incorrect header");
	}
	char* buffer = new char[inbound_data_size];

	dataRead = boost::asio::read(*sock,
			boost::asio::buffer(buffer, inbound_data_size));

	std::istringstream data(std::string(buffer, inbound_data_size));

	boost::archive::text_iarchive archive(data);
	QueueElementTCPSend* dataReceived = new QueueElementTCPSend();
	archive >> dataReceived;
	_recvQueue->Put(dynamic_cast<QueueElementBase*>(dataReceived));

	delete buffer;
	delete headerBuffer;
}

void ConcurrentDataSharer::TCPRecv() {
	for (std::size_t i = 0; i < 10; i++) {

		try {

			boost::asio::ip::tcp::acceptor acceptor(io_service_TCP_recv,
					boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(),
							TCP_recv_port));
			{
				std::unique_lock < std::mutex > lock(*_TCPLock);
				TCP_port_chosen = true;
				TCP_chosen_cond.notify_all();
			}
			while (true) {
				boost::shared_ptr<boost::asio::ip::tcp::socket> sock(
						new boost::asio::ip::tcp::socket(io_service_TCP_recv));
				acceptor.accept(*sock);
				boost::thread t(
						boost::bind(&ConcurrentDataSharer::TCPRecvSession, this,
								sock));
			}
		} catch (boost::exception &e) {
			TCP_recv_port += 1;
		}

	}
	throw std::runtime_error("changed adresses to many times");
}

void ConcurrentDataSharer::TCPSend() {
	{
		logging_buffer->push_back("TCPSend thread created");
	}
	while (true) {
		{
			QueueElementBase* data = _TCPSendQueue->Take();
			{
				logging_buffer->push_back("TCPSend working on data");
			}
			boost::shared_ptr<boost::asio::ip::tcp::socket> socket_TCP_send(
					new boost::asio::ip::tcp::socket(io_service_TCP_recv));
			boost::shared_ptr<boost::asio::ip::tcp::resolver> resolver_TCP(
					new boost::asio::ip::tcp::resolver(io_service_TCP_send));

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
			_clientLock->lock();
			auto it = _clients.find(operation->getName());
			if (it == _clients.end()) {
				{
					std::ostringstream temp;
					temp << "TCPSend could not find client"
							<< operation->getName();
					logging_buffer->push_back(temp.str());
				}
				delete data;
				_clientLock->unlock();
				continue;
			}
			std::vector<std::string> clientadresses = (*it).second->getIPV4();
			port << (*it).second->getPort();
			_clientLock->unlock();
			if (clientadresses.size() == 0) {
				{
					std::ostringstream temp;
					temp << "TCPSend could not find adresses to "
							<< operation->getName();
					logging_buffer->push_back(temp.str());
				}
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

			boost::shared_ptr<std::vector<boost::asio::const_buffer>> buffers(
					new std::vector<boost::asio::const_buffer>);
			buffers->push_back(boost::asio::buffer(outbound_header));
			buffers->push_back(boost::asio::buffer(outbound_data));
			boost::system::error_code ec;

			std::size_t dataSent = boost::asio::write(*socket_TCP_send,
					*buffers, ec);
			io_service_send.run();
			if (!operation->getResponsRequired()) {
				delete data;
			}
			{
				std::ostringstream temp;
				temp << "TCPSend sent data to" << operation->getName()
						<< " with purpose " << operation->getPurpose();
				logging_buffer->push_back(temp.str());
			}
		}
	}
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
