#include <concurrentdatasharer.h>
ConcurrentDataSharer::ConcurrentDataSharer(std::string const& groupName) :
		_groupName(groupName) {
	_multiSendQueue = new BlockingQueue<QueueElementBase*>(255);
	_multiRecvQueue = new BlockingQueue<QueueElementBase*>(255);
	_TCPSendQueue = new BlockingQueue<QueueElementBase*>(255);
	_TCPRecvQueue = new BlockingQueue<QueueElementBase*>(255);
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
}
ConcurrentDataSharer::~ConcurrentDataSharer() {

}

//runs from mainloop, keep minimal
void ConcurrentDataSharer::handleTCPRecv(QueueElementBase* data){
	if(QueueElementSet* operation=dynamic_cast<QueueElementSet*>(data)){
		if(_dataBase.find(operation->getName())==_dataBase.end()){
			//check all other peers if they have this name
			_dataBase[operation->getName()]=new DataBaseElement(operation);
		}
	}
	if(QueueElementGet* operation=dynamic_cast<QueueElementGet*>(data)){
		if(_dataBase.find(operation->getName())==_dataBase.end()){
			//check all other peers if they have this name
			//_dataBase[operation->getName()]=new DataBaseElement(operation);
			operation->setData("");

	}
		else{
			DataBaseElement* element=_dataBase[operation->getName()];
			operation->setData(element->getData());
		}

}}
void ConcurrentDataSharer::handleMultiRecv(QueueElementBase*){

}

void ConcurrentDataSharer::mainLoop() {
	while (true){
		if(!_TCPRecvQueue->Empty()){
			handleTCPRecv(_TCPRecvQueue->Take());
		}
		if(!_multiRecvQueue->Empty()){
			handleMultiRecv(_multiRecvQueue->Take());
		}
	}

}

void ConcurrentDataSharer::TCPRecv() {


}

void ConcurrentDataSharer::TCPSend() {

}

void ConcurrentDataSharer::handleMultiRecvRaw(const boost::system::error_code& error,
	      size_t bytes_recvd){

    if (!error)
    {
      std::cout.write(multiCastData, bytes_recvd);
      std::cout << std::endl;

      socket_recv->async_receive_from(
          boost::asio::buffer(multiCastData, max_length), sender_endpoint_,
          boost::bind(&ConcurrentDataSharer::handleMultiRecvRaw, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
    }

}

void ConcurrentDataSharer::handleMultiSendTimeout(const boost::system::error_code& error){
    if (!error)
    {
      std::ostringstream os;
      os << "Message " << message_count_++;
      message_ = os.str();

      socket_send->async_send_to(
          boost::asio::buffer(message_), *endpoint_,
          boost::bind(&ConcurrentDataSharer::handleMultiSendRaw, this,
            boost::asio::placeholders::error));
    }
}

void ConcurrentDataSharer::handleMultiSendRaw(const boost::system::error_code& error){
	   if (!error && message_count_ < 10)
	    {
	      timer_->expires_from_now(boost::posix_time::seconds(1));
	      timer_->async_wait(
	          boost::bind(&ConcurrentDataSharer::handleMultiSendTimeout, this,
	            boost::asio::placeholders::error));
	    }

}
void ConcurrentDataSharer::MultiSend() {
	const boost::asio::ip::address& multicast_address= boost::asio::ip::address::from_string("239.255.0.1");
	endpoint_=new boost::asio::ip::udp::endpoint(multicast_address, multicast_port);
	 timer_=new boost::asio::deadline_timer(io_service_send);
	 socket_send =new boost::asio::ip::udp::socket(io_service_send, endpoint_->protocol());
	 message_count_=0;
	    std::ostringstream os;
	    os << "Message " << message_count_++;
	    message_ = os.str();
	    socket_send->async_send_to(
	        boost::asio::buffer(message_), *endpoint_,
	        boost::bind(&ConcurrentDataSharer::handleMultiSendRaw, this,
	          boost::asio::placeholders::error));
	    io_service_send.run();
	  }


void ConcurrentDataSharer::MultiRecv() {
	socket_recv=new boost::asio::ip::udp::socket(io_service_recv);
	const boost::asio::ip::address& listen_address=boost::asio::ip::address::from_string("0.0.0.0");
	const boost::asio::ip::address& multicast_address=boost::asio::ip::address::from_string("239.255.0.1");

	boost::asio::ip::udp::endpoint listen_endpoint(
        listen_address, multicast_port);
	socket_recv->open(listen_endpoint.protocol());
    socket_recv->set_option(boost::asio::ip::udp::socket::reuse_address(true));
    socket_recv->bind(listen_endpoint);
    socket_recv->set_option(
        boost::asio::ip::multicast::join_group(multicast_address));

    socket_recv->async_receive_from(
        boost::asio::buffer(multiCastData, max_length), sender_endpoint_,
        boost::bind(&ConcurrentDataSharer::handleMultiRecvRaw, this,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
    io_service_recv.run();
}

