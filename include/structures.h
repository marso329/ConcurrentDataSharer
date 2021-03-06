#ifndef STRUCTURES_H_
#define STRUCTURES_H_

//standard includes
#include <sstream>
#include <string>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <functional>
#include <queue>
#include <stdexcept>

//boost includes
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/assume_abstract.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/thread.hpp>

//my includes
#include "constants.h"

typedef void (*CallbackSig)();



class Subscription{
public:
	Subscription(){};
	virtual void new_value(std::string const& value)=0;
	virtual void end_subscription()=0;
	virtual ~Subscription(){};

};

class ConcurrentDataSharerException:public std::runtime_error{
public:
	ConcurrentDataSharerException(const std::string& what): std::runtime_error(what){

	}

};

template<typename T>
class SubscriptionContainer:public Subscription {
public:
	SubscriptionContainer(std::string const& client, std::string const& name,
			 std::function<void(T)> SubscribeFunc,
			bool skip_conversion=false) :
			_name(name), _client(client), _SubscribeFunc(SubscribeFunc), kill(
					false),_skip_conversion(skip_conversion) {
		_subThread = new boost::thread(
				boost::bind(&SubscriptionContainer::thread_function, this));
	}
	;
	void new_value(std::string const& value) {
		data.push(value);
		{
			std::lock_guard < std::mutex > lk(m);
		}
		cv.notify_one();
	}
	;
	void end_subscription() {
		data.push("");
		kill = true;
		{
			std::lock_guard < std::mutex > lk(m);
		}
		cv.notify_one();

	}
protected:
private:
	void thread_function() {
		while (true) {
			std::unique_lock < std::mutex > lk(m);
			cv.wait_for(lk, std::chrono::milliseconds(1000),
					[this] {return !data.empty();});
			if(kill){
				return;
			}
			while (!data.empty()) {
					std::istringstream ar(data.front());
					data.pop();
					boost::archive::text_iarchive ia(ar);
					T returnData;
					ia >> returnData;
					_SubscribeFunc(returnData);
				}

		}
	}
	std::condition_variable cv;
	std::mutex m;
	std::queue<std::string> data;
	std::string _client;
	std::string _name;
	std::function<void(T)> _SubscribeFunc;
	bool kill;
	bool _skip_conversion;
	boost::thread* _subThread;

};


class logger {
public:
	logger() {
		logging_buffer = new boost::circular_buffer<std::string>(LOGGERSIZE);
		lock = new std::mutex();
	}
	;
	~logger() {
		delete logging_buffer;
		delete lock;
	}
	;
	void push_back(std::string message) {
		lock->lock();
		logging_buffer->push_back(message);
		lock->unlock();
	}
	std::vector<std::string> get_logs() {
		std::vector<std::string> temp;
		lock->lock();
		for (auto it = logging_buffer->begin(); it != logging_buffer->end();
				it++) {
			temp.push_back(*it);
		}
		lock->unlock();
		return temp;
	}
private:
	//logging
	boost::circular_buffer<std::string>* logging_buffer;
	std::mutex* lock;
};

class QueueElementBase {
public:
	QueueElementBase();
	virtual ~QueueElementBase() {
	}
	;
	std::string getName() {
		return _name;
	}
	std::string getData() {
		return _data;
	}
protected:
	std::string _name;
	std::string _data;
private:
};

class QueueElementSet: public QueueElementBase {
public:
	QueueElementSet(std::string const&, std::string const&);
	~QueueElementSet();
private:
protected:
};

enum MultiSend {
	MULTIUNDEFINED, MULTIINTRODUCTION,MULTICHECKNAME
};

class QueueElementMultiSend: public QueueElementBase {
public:
	QueueElementMultiSend(std::string const&, std::string const&, MultiSend);
	QueueElementMultiSend();
	MultiSend getPurpose();

	~QueueElementMultiSend();
private:
	friend class boost::serialization::access;
	template<class Archive>
	void serialize(Archive & ar, const unsigned int version) {
		ar & _name;
		ar & _data;
		ar & _purpose;
	}
	MultiSend _purpose;
protected:
};

enum TCPSend {
	TCPUNDEFINED,
	TCPSENDVARIABLES,
	TCPREPLYVARIABLES,
	TCPPERSONALINTRODUCTION,
	TCPGETVARIABLE,
	TCPREPLYGETVARIABLE,
	TCPSENDCLIENTS,
	TCPREPLYCLIENTS,
	TCPPING,
	TCPPINGREPLY,
	TCPSTARTSUBSCRIPTION,
	TCPSUBSCRIPTION,
	TCPNAMETAKEN
};

class clientData;
class QueueElementTCPSend: public QueueElementBase {
public:
	QueueElementTCPSend(std::string const& name, std::string const& data,
			TCPSend purpose, bool respons,bool builtin_client=false) :
			_responsRequired(respons) {
		_name = name;
		_data = data;
		_purpose = purpose;
		_tag = "";
		_builtin_client=builtin_client;
		_client=NULL;

	}
	QueueElementTCPSend() :
			_responsRequired(false) {
		_name = "";
		_data = "";
		_purpose = TCPUNDEFINED;
		_tag = "";
		_builtin_client=false;
		_client=NULL;
	}
	;
	bool builtinClient(){
		return _builtin_client;
	}
	clientData* getClient(){
		return _client;
	}
	void setClient(clientData* client){
		_client=client;
	}
	void setTag(std::string const& tag) {
		_tag = tag;
	}
	std::string getTag() {
		return _tag;
	}
	bool getResponsRequired() {
		return _responsRequired;
	}
	TCPSend getPurpose() {
		return _purpose;
	}
	;
	void setRequestor(std::string const& requestor) {
		_requestor = requestor;
	}

	std::string getRequestor() {
		return _requestor;
	}
	void setData(std::string const& data) {
		_data = data;
		{
			std::lock_guard < std::mutex > lk(m);
			ready = true;
		}
		cv.notify_one();
	}
	std::string getData() {
		std::unique_lock < std::mutex > lk(m);
		cv.wait(lk, [this] {return ready;});
		return _data;
	}
	std::string getDataNoneBlocking() {
		return _data;
	}

	~QueueElementTCPSend() {
	}
	;
private:
	friend class boost::serialization::access;
	template<class Archive>
	void serialize(Archive & ar, const unsigned int version) {
		ar & _name;
		ar & _data;
		ar & _purpose;
		ar & _tag;
		ar & _requestor;
	}
	TCPSend _purpose;
	const bool _responsRequired;
	std::string _tag;
	std::string _requestor;
	std::condition_variable cv;
	bool ready = false;
	std::mutex m;
	clientData* _client;
	bool _builtin_client;
protected:
};

class QueueElementGet: public QueueElementBase {
public:
	QueueElementGet(std::string const&);
	std::string getData();
	void setData(std::string const&);
	~QueueElementGet();
private:
	std::condition_variable cv;
	bool ready = false;
	std::mutex m;
protected:
};

class QueueElementCallback: public QueueElementBase {
public:
	QueueElementCallback(std::string const& name, CallbackSig func) {
		_name = name;
		callback = func;
	}
	CallbackSig getCallback() {
		return callback;
	}
	~QueueElementCallback() {
	}
	;
private:
	CallbackSig callback;
protected:
};

class QueueElementSubscribe: public QueueElementBase {
public:
	QueueElementSubscribe(std::string const& client, std::string const& var,
			std::function<void(const std::string&)> func) {
		_name = client;
		callback = func;
		variable = var;
	}
	std::function<void(const std::string&)> getCallback() {
		return callback;
	}
	std::string getVar() {
		return variable;
	}
	~QueueElementSubscribe() {
	}
	;
private:
	std::function<void(const std::string&)> callback;
	std::string variable;

protected:
};

class DataBaseElement {
public:
	DataBaseElement(std::string const&, std::string const&);
	DataBaseElement(QueueElementSet*);
	~DataBaseElement();
	std::string getData();
	void setData(std::string const& data) {
		_data = data;
	}
	void setCallback(CallbackSig func) {
		callback = func;
	}
	void runCallback() {
		if (callback != NULL) {
			callback();
		}
	}
	std::vector<std::string> subscribers;
protected:
private:
	std::string _name;
	std::string _data;
	CallbackSig callback;

};

class clientData {
public:
	clientData() :
			_TCP_port(0) {
		last_ping = std::chrono::high_resolution_clock::now();
	}
	;
	clientData(std::string name, std::vector<std::string> IPV4,
			std::vector<std::string> IPV6, short unsigned TCP_port) :
			_name(name), IPV4Adresses(IPV4), IPV6Adresses(IPV6), _TCP_port(
					TCP_port) {
		last_ping = std::chrono::high_resolution_clock::now();
	}
	;
	clientData(const clientData& data) :
			_name(data._name), IPV4Adresses(data.IPV4Adresses), IPV6Adresses(
					data.IPV6Adresses), _TCP_port(data._TCP_port) {
		last_ping = std::chrono::high_resolution_clock::now();
	}

	clientData(const clientData* data) :
			_name(data->_name), IPV4Adresses(data->IPV4Adresses), IPV6Adresses(
					data->IPV6Adresses), _TCP_port(data->_TCP_port) {
		last_ping = std::chrono::high_resolution_clock::now();
	}

	std::vector<std::string> getIPV4() {
		return IPV4Adresses;
	}
	std::vector<std::string> getIPV6() {
		return IPV6Adresses;
	}
	std::string getName() {
		return _name;
	}
	unsigned short getPort() {
		return _TCP_port;
	}
	~clientData() {
	}
	;
	std::chrono::time_point<std::chrono::high_resolution_clock> last_ping;
	std::size_t ping_failure_counter = 0;
protected:
private:
	friend class boost::serialization::access;
	template<class Archive>
	void serialize(Archive & ar, const unsigned int version) {
		ar & _name;
		ar & IPV4Adresses;
		ar & IPV6Adresses;
		ar & _TCP_port;
	}
	std::string _name;
	std::vector<std::string> IPV4Adresses;
	std::vector<std::string> IPV6Adresses;
	short unsigned _TCP_port;
};

#endif
