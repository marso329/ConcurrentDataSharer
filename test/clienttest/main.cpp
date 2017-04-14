#include "concurrentdatasharer.h"
#include <iostream>
#include <unistd.h>

int main(int argc, char ** argv) {
	ConcurrentDataSharer* sharer = new ConcurrentDataSharer("test");
	sharer->set<int>("hello", 32);
	int value = sharer->get<int>("hello");
	std::cout << "value: " << value << std::endl;

	sharer->set<std::string>("test", "dawdawdawd");

	std::string test = sharer->get<std::string>("test");

	std::cout << "test: " << test << std::endl;
	usleep(100000000);
}
