#include "concurrentdatasharer.h"
#include <iostream>
#include <unistd.h>

void newClient() {
	std::cout << "hello has changes" << std::endl;
}

int main(int argc, char ** argv) {
	ConcurrentDataSharer* sharer = new ConcurrentDataSharer("test");
	std::cout<<"my name is"<<sharer->getMyName()<<" and I just share two variables"<<std::endl;
	sharer->set<int>("data",43);
	sharer->set<int>("data1",42);
	usleep(100000000);
}
