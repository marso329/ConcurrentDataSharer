#ifndef PYTHON_H_
#define PYTHON_H_

#include <boost/python.hpp>
#include "concurrentdatasharer.h"
#include <boost/python/list.hpp>
using namespace boost::python;

class ConcurrentDataSharerPython:public ConcurrentDataSharer{
public:
	ConcurrentDataSharerPython(std::string const & groupname,
			std::string const & multicastadress = "239.255.0.1",
			std::string const & listenadress = "0.0.0.0",
			const short multicastport = 30001):ConcurrentDataSharer(groupname,multicastadress,listenadress,multicastport){
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



protected:
private:
};

BOOST_PYTHON_MODULE(ConcurrentDataSharer)
{
    class_<ConcurrentDataSharerPython,boost::noncopyable>("ConcurrentDataSharer",init<std::string,std::string,std::string,const short>())
   .def(init<std::string>())
		   .def("setValue", &ConcurrentDataSharerPython::set<int>,
        return_value_policy<reference_existing_object>()).def("getClients",&ConcurrentDataSharerPython::getClientsPython)
		.def("getClientVariablesList",&ConcurrentDataSharerPython::getClientVariablesListPython).def("getClientVariable",&ConcurrentDataSharerPython::getClientVariableIntPython);

}


#endif
