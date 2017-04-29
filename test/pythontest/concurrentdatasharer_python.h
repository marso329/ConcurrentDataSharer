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

	boost::python::list getClientVariablesListPython(std::string const& client){
		std::vector<std::string> var=getClientVariables(client);
		boost::python::list temp;
		for (auto it =var.begin();it!=var.end();it++){
			temp.append(*it);
		}
		return temp;
	}

	int getClientVariableIntPython(std::string const& client,std::string const& var){
		return get<int>(client,var);
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
		.def("getValue",&ConcurrentDataSharerPython::getValuePython);

}


#endif
