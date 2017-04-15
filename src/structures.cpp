#include "structures.h"

QueueElementBase::QueueElementBase() {

}

QueueElementSet::QueueElementSet(std::string const& name,
		std::string const& data) :
		QueueElementBase() {
	_name = name;
	_data = data;
}

QueueElementSet::~QueueElementSet() {

}

QueueElementGet::QueueElementGet(std::string const& name) :
		QueueElementBase() {
	_name = name;
}

QueueElementGet::~QueueElementGet() {

}
std::string QueueElementGet::getData() {
	std::unique_lock < std::mutex > lk(m);
	cv.wait(lk, [this] {return ready;});
	return _data;
}
void QueueElementGet::setData(std::string const& data) {
	_data = data;
	{
		std::lock_guard < std::mutex > lk(m);
		ready = true;
	}
	cv.notify_one();
}

DataBaseElement::DataBaseElement(std::string const& name,
		std::string const& data) :
		_name(name), _data(data),callback(NULL) {

}
DataBaseElement::DataBaseElement(QueueElementSet* element) :
		_name(element->getName()), _data(element->getData()),callback(NULL) {

}

DataBaseElement::~DataBaseElement() {

}
std::string DataBaseElement::getData() {
	return _data;
}

QueueElementMultiSend::QueueElementMultiSend(std::string const& name,
		std::string const& data,MultiSend purpose):_purpose(purpose) {
	_name = name;
	_data = data;
}


QueueElementMultiSend::QueueElementMultiSend():_purpose(MultiSend::UNDEFINED) {
	_name = "";
	_data = "";
}

MultiSend QueueElementMultiSend::getPurpose(){
return _purpose;
}
QueueElementMultiSend::~QueueElementMultiSend() {

}
