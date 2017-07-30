#include "concurrentdatasharer.h"
#include <iostream>
#include <unistd.h>

void sub(int data){
	std::cout<<"subscription: "<<data<<std::endl;
}

int main(int argc, char ** argv) {
	ConcurrentDataSharer* sharer = new ConcurrentDataSharer("test",ConcurrentDataSharer::default_multicastadress,ConcurrentDataSharer::default_listenadress,ConcurrentDataSharer::default_multicastport);
	sleep(1);
	ConcurrentDataSharer* subscriber = new ConcurrentDataSharer("test",ConcurrentDataSharer::default_multicastadress,ConcurrentDataSharer::default_listenadress,ConcurrentDataSharer::default_multicastport);

	sharer->set<int>("data",1234);
	std::string sharerName=sharer->getMyName();
std::cout<<"init done"<<std::endl;
sleep(1);
subscriber->subscribe(sharerName,"data",sub);
std::cout<<"started sub"<<std::endl;
sleep(1);
sharer->set("data",4321);
std::cout<<"changed data"<<std::endl;
sleep(3);
std::vector<std::string> logs=sharer->getLogger();
std::cout<<"sharer logs"<<std::endl;
for(auto it =logs.begin();it!=logs.end();it++){
	std::cout<<*it<<std::endl;
}
std::cout<<std::endl;
std::cout<<std::endl;
std::cout<<std::endl;
logs=subscriber->getLogger();
std::cout<<"subscriber logs"<<std::endl;
for(auto it =logs.begin();it!=logs.end();it++){
	std::cout<<*it<<std::endl;
}


}
