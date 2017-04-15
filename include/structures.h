#ifndef STRUCTURES_H_
#define STRUCTURES_H_
#include <string>
#include <condition_variable>
#include <mutex>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <sstream>


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

class DataBaseElement {
public:
	DataBaseElement(std::string const&, std::string const&);
	DataBaseElement(QueueElementSet*);
	~DataBaseElement();
	std::string getData();
protected:
private:
	std::string _name;
	std::string _data;

};

#endif
