#include <concurrentdatasharer.h>
ConcurrentDataSharer::ConcurrentDataSharer(std::string const& groupName,
		std::string const& multicastadress, std::string const & listenadress,
		const short multicastport) :
		_groupName(groupName), multicast_address(
				boost::asio::ip::address::from_string(multicastadress)), listen_address(
				boost::asio::ip::address::from_string(listenadress)), multicast_port(
				multicastport),_name(generateRandomName(10)) {
	_multiSendQueue = new BlockingQueue<QueueElementBase*>(255);
	_recvQueue = new BlockingQueue<QueueElementBase*>(255);
	_TCPSendQueue = new BlockingQueue<QueueElementBase*>(255);


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
	QueueElementMultiSend* data = new QueueElementMultiSend(_name, "",
			INTRODUCTION);
	_multiSendQueue->Put(dynamic_cast<QueueElementBase*>(data));
}

//runs from mainloop, keep minimal
void ConcurrentDataSharer::handleTCPRecv(QueueElementBase* data) {
	if (QueueElementSet* operation = dynamic_cast<QueueElementSet*>(data)) {
		if (_dataBase.find(operation->getName()) == _dataBase.end()) {
			//check all other peers if they have this name
			_dataBase[operation->getName()] = new DataBaseElement(operation);
		}
	}
	if (QueueElementGet* operation = dynamic_cast<QueueElementGet*>(data)) {
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
void ConcurrentDataSharer::handleMultiRecv(QueueElementBase* data) {
	QueueElementMultiSend* package=dynamic_cast<QueueElementMultiSend*>(data);
	switch(package->getPurpose()){
	case UNDEFINED:{
		throw std::runtime_error("undefined multisend package");
		break;
	}
	case INTRODUCTION:{
		std::cout<<"Node: "<<package->getName()<<" joined"<<std::endl;
		break;
	}
	default:{
		throw std::runtime_error("shit is fucked");
		break;
	}


	};

}

void ConcurrentDataSharer::mainLoop() {
	while (true) {
		QueueElementBase* data =_TCPSendQueue->Take();

		if(QueueElementSet* package=dynamic_cast<QueueElementSet*>(data)){
			handleTCPRecv(data);
		}
		else if(QueueElementGet* package=dynamic_cast<QueueElementGet*>(data)){
			handleTCPRecv(data);
		}
		else if(QueueElementMultiSend* package=dynamic_cast<QueueElementMultiSend*>(data)){
			handleMultiRecv(data);
		}
		else{
			throw std::runtime_error("no casting in mainloop succesfull");
		}
	}

}

void ConcurrentDataSharer::TCPRecv() {
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
		delete element;
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
		_TCPSendQueue->Put(dynamic_cast<QueueElementBase*>(dataReceived));

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

std::string ConcurrentDataSharer::generateRandomName(std::size_t size){
	const char alphanum[] =
	"0123456789"
	"!@#$%^&*"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz";

	int stringLength = sizeof(alphanum) - 1;
	srand (time(NULL));
	std::string name;
	for (std::size_t i=0;i<size;i++){
		name+=alphanum[rand()%stringLength];
	}
	return name;

}

