#ifndef PYTHON_H_
#define PYTHON_H_

#include <boost/python.hpp>
#include "concurrentdatasharer.h"
using namespace boost::python;

BOOST_PYTHON_MODULE(libConcurrentDataSharer)
{
    class_<ConcurrentDataSharer,boost::noncopyable>("ConcurrentDataSharer",init<std::string,std::string,std::string,const short>())
   .def(init<std::string>())
		   .def("setValue", &ConcurrentDataSharer::set<int>,
        return_value_policy<reference_existing_object>()) 
    ;
}


#endif
