#ifndef PYTHON_H_
#define PYTHON_H_

#include <boost/python.hpp>
#include "concurrentdatasharer.h"
#include <boost/python/list.hpp>
using namespace boost::python;

struct Pickler {
  object module;
  object dumps;
  object loads;
};


class ConcurrentDataSharerPython:public ConcurrentDataSharer{
public:
	ConcurrentDataSharerPython(std::string const & groupname,
			std::string const & multicastadress = "239.255.0.1",
			std::string const & listenadress = "0.0.0.0",
			const short multicastport = 30001):ConcurrentDataSharer(groupname,multicastadress,listenadress,multicastport){
		 pickler = new Pickler();
		  pickler->module = object(handle<>(PyImport_ImportModule("pickle")));
		  pickler->dumps = pickler->module.attr("dumps");
		  pickler->loads = pickler->module.attr("loads");
	};
	boost::python::list getClientsPython(){
		boost::python::list temp;
		std::vector<std::string> clients=getClients();
		for (auto it =clients.begin();it!=clients.end();it++){
			temp.append(*it);
		}
		return temp;
	}
	boost::python::list getLogsPython(){
		boost::python::list temp;
		std::vector<std::string> clients=getLogger();
		for (auto it =clients.begin();it!=clients.end();it++){
			std::string temp_string=*it;
				//boost::python::str temp_python_string(temp_string);
				temp.append( handle<>( PyBytes_FromString( temp_string.c_str() ) ) );
		//	temp.append(*it);
		}
		return temp;
	}

	boost::python::list getClientVariablesListPython(std::string const& client){
		std::vector<std::string> var=getClientVariables(client);
		boost::python::list temp;
		for (auto it =var.begin();it!=var.end();it++){
			temp.append(*it);
		}
		return temp;
	}

	object getClientVariableIntPython(std::string const& client,std::string const& var){
		QueueElementTCPSend * toSend = new QueueElementTCPSend(client, var,
				TCPGETVARIABLE, true);
		_TCPSendQueue->Put(dynamic_cast<QueueElementBase*>(toSend));
		std::string data = toSend->getData();
		delete toSend;

		return ((pickler->loads)(object(data).attr("encode")()));
	}

	void setValuePython(std::string const& name,boost::python::object& obj){
		  std::string tempdata= extract<std::string>((pickler->dumps)(obj, 1));
			QueueElementSet* element = new QueueElementSet(name, tempdata);
			_recvQueue->Put(element);
	}

	object getValuePython(std::string const& name){
		QueueElementGet* element = new QueueElementGet(name);
		_recvQueue->Put(element);
		std::string tempData = element->getData();
		return ((pickler->loads)(object(tempData).attr("encode")()));
	}

	void subscribePython(std::string const& client,std::string const& var,object func){
		std::string object_classname = boost::python::extract<std::string>(func.attr("__class__").attr("__name__"));
		if(object_classname!="function"){
			throw std::runtime_error("must pass function");
		}
		std::function<void(const std::string&)> subFunc =createPythonSubscription(func);
		QueueElementSubscribe* toSend=new QueueElementSubscribe(client,var,subFunc);
		_recvQueue->Put(toSend);
		_subscription[client+var]=subFunc;

	}

	std::function<void(const std::string&)> createPythonSubscription(
			object) {
		return [=](const std::string& data) {object(((pickler->loads)(object(data).attr("encode")())));
			return;};
	}
	;
	void printLogs(){
		std::vector<std::string> clients=getLogger();
		for (auto it =clients.begin();it!=clients.end();it++){
			std::cout<<*it<<std::endl;
		}

	}




protected:
private:
	Pickler* pickler;
};

BOOST_PYTHON_MODULE(ConcurrentDataSharer)
{
    class_<ConcurrentDataSharerPython,boost::noncopyable>("ConcurrentDataSharer",init<std::string,std::string,std::string,const short>())
   .def(init<std::string>())
		   .def("setValue", &ConcurrentDataSharerPython::setValuePython,
        return_value_policy<reference_existing_object>()).def("getClients",&ConcurrentDataSharerPython::getClientsPython)
		.def("getClientVariablesList",&ConcurrentDataSharerPython::getClientVariablesListPython).def("getClientVariable",&ConcurrentDataSharerPython::getClientVariableIntPython)
		.def("getValue",&ConcurrentDataSharerPython::getValuePython).def("getLogs",&ConcurrentDataSharerPython::getLogsPython)
		.def("subscribe",&ConcurrentDataSharerPython::subscribePython).def("printLogs",&ConcurrentDataSharerPython::printLogs).def("getName",&ConcurrentDataSharerPython::getMyName);

}


#endif
