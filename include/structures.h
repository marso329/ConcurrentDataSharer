#ifndef STRUCTURES_H_
#define STRUCTURES_H_
#include <string>
#include <condition_variable>
#include <mutex>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/assume_abstract.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <sstream>


typedef void (* CallbackSig)();

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

enum MultiSend { UNDEFINED, INTRODUCTION};

class QueueElementMultiSend: public QueueElementBase {
public:
	QueueElementMultiSend(std::string const&, std::string const&,MultiSend);
	QueueElementMultiSend();
	MultiSend getPurpose();

	~QueueElementMultiSend();
private:
	friend class boost::serialization::access;
	template<class Archive>
	void serialize(Archive & ar, const unsigned int version) {
		ar & _name;
		ar & _data;
		ar& _purpose;
	}
	MultiSend _purpose;
protected:
};



class QueueElementTCPRecv: public QueueElementBase {
public:
	QueueElementTCPRecv(std::string const& name, std::string const& data){
		_name=name;
		_data=data;
	}
	QueueElementTCPRecv(){
		_name="";
		_data="";
	};

	~QueueElementTCPRecv(){};
private:
	friend class boost::serialization::access;
	template<class Archive>
	void serialize(Archive & ar, const unsigned int version) {
		ar & _name;
		ar & _data;
	}
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
	QueueElementCallback(std::string const& name,CallbackSig func){
_name=name;
	callback=func;
	}
	CallbackSig getCallback(){
		return callback;
	}
	~QueueElementCallback(){};
private:
	CallbackSig callback;
protected:
};

class DataBaseElement {
public:
	DataBaseElement(std::string const&, std::string const&);
	DataBaseElement(QueueElementSet*);
	~DataBaseElement();
	std::string getData();
	void setData(std::string const& data){
		_data=data;
	}
	void setCallback(CallbackSig func){
		callback=func;
	}
	void runCallback(){
		if(callback!=NULL){
			callback();
		}
	}
protected:
private:
	std::string _name;
	std::string _data;
	CallbackSig callback;

};

class clientData{
public:
	clientData(){};
	clientData(std::string name,std::vector<std::string> IPV4,std::vector<std::string> IPV6):_name(name),IPV4Adresses(IPV4),IPV6Adresses(IPV6){};
	std::vector<std::string> getIPV4(){
		return IPV4Adresses;
	}
	std::vector<std::string> getIPV6(){
		return IPV6Adresses;
	}
	std::string getName(){
		return _name;
	}
	~clientData(){};
protected:
private:
	friend class boost::serialization::access;
	template<class Archive>
	void serialize(Archive & ar, const unsigned int version) {
		ar & _name;
		ar & IPV4Adresses;
		ar& IPV6Adresses;
	}
	std::string _name;
	std::vector<std::string> IPV4Adresses;
	std::vector<std::string> IPV6Adresses;
};

#endif
