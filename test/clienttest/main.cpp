#include "concurrentdatasharer.h"
#include <iostream>
#include <unistd.h>

void newClient() {
	std::cout << "hello has changes" << std::endl;
}

int main(int argc, char ** argv) {
	ConcurrentDataSharer* sharer = new ConcurrentDataSharer("test");

	std::cout<<"My name is : "<<sharer->getMyName()<<" and i want a list of a nother clients variable"<<std::endl;

	usleep(1000000);
	/**
	while (true) {
		std::vector<std::string> clients = sharer->getClients();
		if (clients.size() != 1) {
			break;
		}
		usleep(1000);
	}
	std::cout<<"NEW client"<<std::endl;
	std::vector<std::string> clients = sharer->getClients();
	for (auto it = clients.begin(); it != clients.end(); it++) {
		if (*it != sharer->getMyName()) {
			std::vector < std::string > var= sharer->getClientVariables(*it);
			std::cout<<*it<<"has variables:"<<std::endl;
			for(auto ti=var.begin();ti!=var.end();ti++){
				std::cout<<*ti<<std::endl;
			}
		}
	}
**/
}
