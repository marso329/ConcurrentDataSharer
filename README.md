# ConcurrentDataSharer
A library for concurrently sharing data between software. Any datatype that can be serialized using
boost::serialization can be shared such as all STL datatypes. It finds other computers using ConcurrentDataSharer by
broadcasting over UDP.

It could for example be used for a smart home where one computer which controls a lamp shares a bool and have a callback connected to it. When the variable is changed from another computer it turns the lamp on or off.

## Getting Started

### Prerequisites
Tested on Ubuntu 17.04
```
sudo apt-get install git cmake libboost-all-dev
```

### Build
```
cmake .
make
```

### Running the tests
## C++ test
In one terminal
```
./bin/main
```
In another
```
./bin/main1
```
## Python test
In one terminal
```
cd ./test/pythontest
python3 test.py
```
In another terminal

```
cd ./test/pythontest
python3 test.py
```
