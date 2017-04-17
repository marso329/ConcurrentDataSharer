cmake_minimum_required (VERSION 3.5.1)
project (pythontest)
set(CMAKE_BUILD_TYPE Release)
# The version number.
set (Tutorial_VERSION_MAJOR 1)
set (Tutorial_VERSION_MINOR 0)

include_directories(../../include /usr/include/python2.7/)

file(GLOB SOURCES "../../src/*.cpp")
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ../lib)
set (CMAKE_CXX_FLAGS "-std=c++11")

add_library(ConcurrentDataSharer SHARED concurrentdatasharer_python.cpp ${SOURCES})
target_link_libraries(ConcurrentDataSharer -lpython2.7 -lboost_python -lboost_serialization -lboost_thread -lboost_system )

#include_directories(../../include /usr/include/python3.5/)
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../../bin)
set (CMAKE_CXX_FLAGS "-std=c++11")
#add_executable(main1 main.cpp)

#add_library(ConcurrentDataSharer SHARED  concurrentdatasharer_python.cpp)
#target_link_libraries(ConcurrentDataSharer ${PROJECT_LINK_LIBS} -lConcurrentDataSharer  -L/usr/lib/python3.5/config-3.5m-x86_64-linux-gnu -lpython3.5 -lboost_python-py35 -lboost_serialization  -lboost_thread -lboost_system  )
