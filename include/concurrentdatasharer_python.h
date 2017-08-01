#ifndef PYTHON_H_
#define PYTHON_H_

#include <boost/python.hpp>
#include "concurrentdatasharer.h"
#include <boost/python/list.hpp>
#include <boost/python/call.hpp>
#include <boost/shared_ptr.hpp>
using namespace boost::python;

struct Pickler {
	object module;
	object dumps;
	object loads;
};

class with_gil {
public:
	with_gil() {
		state_ = PyGILState_Ensure();
	}
	~with_gil() {
		PyGILState_Release(state_);
	}

	with_gil(const with_gil&) = delete;
	with_gil& operator=(const with_gil&) = delete;
private:
	PyGILState_STATE state_;
};

class without_gil {
public:
	without_gil() {
		state_ = PyEval_SaveThread();
	}
	~without_gil() {
		PyEval_RestoreThread(state_);
	}

	without_gil(const without_gil&) = delete;
	without_gil& operator=(const without_gil&) = delete;
private:
	PyThreadState* state_;
};

template<typename T>
class SubscriptionContainerPython: public Subscription {
public:
	SubscriptionContainerPython(std::string const& client,
			std::string const& name, std::function<void(T)> SubscribeFunc,
			bool skip_conversion = false) :
			_name(name), _client(client), _SubscribeFunc(SubscribeFunc), kill(
					false), _skip_conversion(skip_conversion), _subThread(NULL) {

	}
	;
	void new_value(std::string const& value) {
		data.push(value);
		_subThread = new boost::thread(
				boost::bind(&SubscriptionContainerPython::thread_function,
						this));
		_subThread->join();
		delete _subThread;
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
		PyInterpreterState *istate = PyInterpreterState_Head();
		PyThreadState *tstate = PyThreadState_New(istate);
		PyGILState_STATE state;
		while (!data.empty()) {
			std::string temp = data.front();
			data.pop();
			state = PyGILState_Ensure();
			try{
			_SubscribeFunc(temp);
			}
			catch (error_already_set& e) {
				PyErr_Print();
			}
			PyGILState_Release(state);
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
	//PyThread_type_lock lock;

};

typedef void (*StringSubFunc)(const std::string&);

std::ostream& operator<<(std::ostream& os, const boost::python::object& o) {
	return os << boost::python::extract<std::string>(boost::python::str(o))();
}

class ConcurrentDataSharerPython: public ConcurrentDataSharer {
public:
	ConcurrentDataSharerPython(std::string const & groupname) :
			ConcurrentDataSharer(groupname,ConcurrentDataSharer::default_multicastadress,ConcurrentDataSharer::default_listenadress,ConcurrentDataSharer::default_multicastport) {
		//Py_Initialize();
		main = import("__main__");
		main_namespace = main.attr("__dict__");
		pickler = new Pickler();
		pickler->module = object(handle<>(PyImport_ImportModule("pickle")));
		pickler->dumps = pickler->module.attr("dumps");
		pickler->loads = pickler->module.attr("loads");
		threadState = PyGILState_GetThisThreadState();
	}
	;
	ConcurrentDataSharerPython(std::string const & groupname,std::string const& name) :
			ConcurrentDataSharer(groupname,name,ConcurrentDataSharer::default_multicastadress,ConcurrentDataSharer::default_listenadress,ConcurrentDataSharer::default_multicastport) {
		//Py_Initialize();
		main = import("__main__");
		main_namespace = main.attr("__dict__");
		pickler = new Pickler();
		pickler->module = object(handle<>(PyImport_ImportModule("pickle")));
		pickler->dumps = pickler->module.attr("dumps");
		pickler->loads = pickler->module.attr("loads");
		threadState = PyGILState_GetThisThreadState();
	}
	;


	boost::python::list getClientsPython() {
		boost::python::list temp;
		std::vector<std::string> clients = getClients();
		for (auto it = clients.begin(); it != clients.end(); it++) {
			temp.append(*it);
		}
		return temp;
	}
	boost::python::list getLogsPython() {
		boost::python::list temp;
		std::vector<std::string> clients = getLogger();
		for (auto it = clients.begin(); it != clients.end(); it++) {
			std::string temp_string = *it;
			//boost::python::str temp_python_string(temp_string);
			temp.append(handle<>(PyBytes_FromString(temp_string.c_str())));
			//	temp.append(*it);
		}
		return temp;
	}

	boost::python::list getClientVariablesListPython(
			std::string const& client) {
		std::vector<std::string> var = getClientVariables(client);
		boost::python::list temp;
		for (auto it = var.begin(); it != var.end(); it++) {
			temp.append(*it);
		}
		return temp;
	}

	object getClientVariableIntPython(std::string const& client,
			std::string const& var) {
		QueueElementTCPSend * toSend = new QueueElementTCPSend(client, var,
				TCPGETVARIABLE, true);
		_TCPSendQueue->Put(dynamic_cast<QueueElementBase*>(toSend));
		std::string data = toSend->getData();
		delete toSend;

		return ((pickler->loads)(object(data).attr("encode")()));
	}

	void setValuePython(std::string const& name, boost::python::object& obj) {
		std::string tempdata = extract<std::string>((pickler->dumps)(obj, 1));
		QueueElementSet* element = new QueueElementSet(name, tempdata);
		_recvQueue->Put(element);
	}

	object getValuePython(std::string const& name) {
		QueueElementGet* element = new QueueElementGet(name);
		_recvQueue->Put(element);
		std::string tempData = element->getData();
		return ((pickler->loads)(object(tempData).attr("encode")()));
	}

	object dePicle(std::string const& data) {
		std::cout << data << std::endl;
		boost::python::str temp = "adawd";
		return temp;
		//return ((pickler->loads)(object(data).attr("encode")()));
	}

	void subscribePython(std::string const& client, std::string const& var,
			object func) {
		std::string object_classname = boost::python::extract<std::string>(
				func.attr("__class__").attr("__name__"));

		std::string object_funcname = boost::python::extract<std::string>(
				func.attr("__name__"));
		//if (object_classname != "function" &&object_classname!="method") {
		if(!PyCallable_Check(func.ptr())){
			throw std::runtime_error("must pass function not"+object_classname);
		}
		object_funcname=generateRandomName(20);
		main_namespace[object_funcname]=func;
		//create subscription function that converts std::string to object
		std::function<void(std::string)> subFunc = createPythonSubscription(
				object_funcname);
		//start a subscription
		QueueElementSubscribe* toSend = new QueueElementSubscribe(client, var,
				subFunc);
		PyGILState_STATE state = PyGILState_Ensure();
		_subscription[client + var] = new SubscriptionContainerPython<
				std::string>(client, var, subFunc, true);
		PyGILState_Release(state);
		_recvQueue->Put(toSend);

	}
	std::function<void(std::string)> createPythonSubscription(const std::string func) {
		return [&,func](std::string data) {main_namespace[func](pickler->loads(object(data)));};
	}
	;
	void printLogs() {
		std::vector<std::string> clients = getLogger();
		for (auto it = clients.begin(); it != clients.end(); it++) {
			std::cout << *it << std::endl;
		}

	}

protected:
private:
	Pickler* pickler;
	object main;
	object main_namespace;
	PyThreadState* threadState;
};

BOOST_PYTHON_MODULE(ConcurrentDataSharerPython3)
{
	Py_Initialize();
	PyEval_InitThreads();
	class_<ConcurrentDataSharerPython, boost::noncopyable>(
			"ConcurrentDataSharer",init<std::string>()).def(
							init<std::string,std::string>()).def("setValue",
			&ConcurrentDataSharerPython::setValuePython,
			return_value_policy<reference_existing_object>()).def("getClients",
			&ConcurrentDataSharerPython::getClientsPython).def(
			"getClientVariablesList",
			&ConcurrentDataSharerPython::getClientVariablesListPython).def(
			"getClientVariable",
			&ConcurrentDataSharerPython::getClientVariableIntPython).def(
			"getValue", &ConcurrentDataSharerPython::getValuePython).def(
			"getLogs", &ConcurrentDataSharerPython::getLogsPython).def(
			"subscribe1", &ConcurrentDataSharerPython::subscribePython).def(
			"printLogs", &ConcurrentDataSharerPython::printLogs).def("getName",
			&ConcurrentDataSharerPython::getMyName).def("subscribe",&ConcurrentDataSharerPython::subscribePython);

}

BOOST_PYTHON_MODULE(ConcurrentDataSharerPython2)
{
	Py_Initialize();
	PyEval_InitThreads();
	class_<ConcurrentDataSharerPython, boost::noncopyable>(
			"ConcurrentDataSharer",init<std::string>()).def(
							init<std::string,std::string>()).def("setValue",
			&ConcurrentDataSharerPython::setValuePython,
			return_value_policy<reference_existing_object>()).def("getClients",
			&ConcurrentDataSharerPython::getClientsPython).def(
			"getClientVariablesList",
			&ConcurrentDataSharerPython::getClientVariablesListPython).def(
			"getClientVariable",
			&ConcurrentDataSharerPython::getClientVariableIntPython).def(
			"getValue", &ConcurrentDataSharerPython::getValuePython).def(
			"getLogs", &ConcurrentDataSharerPython::getLogsPython).def(
			"subscribe1", &ConcurrentDataSharerPython::subscribePython).def(
			"printLogs", &ConcurrentDataSharerPython::printLogs).def("getName",
			&ConcurrentDataSharerPython::getMyName).def("subscribe",&ConcurrentDataSharerPython::subscribePython);

}

#endif
