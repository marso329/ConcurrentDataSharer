#include "concurrentdatasharer.h"
#include <iostream>
#include <unistd.h>

	void newClient(){
	std::cout<<"hello has changes"<<std::endl;
	}

int main(int argc, char ** argv) {
	ConcurrentDataSharer* sharer = new ConcurrentDataSharer("test");
	sharer->set<int>("hello", 32);
	int value = sharer->get<int>("hello");
	sharer->registerCallback("hello",&newClient);
	sharer->set<int>("hello", 42);
	value = sharer->get<int>("hello");

std::cout<<"value"<<value<<std::endl;
	usleep(100000000);
}
