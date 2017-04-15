cmake_minimum_required (VERSION 3.5.1)
project (ConcurrentDataSharer)
set(CMAKE_BUILD_TYPE Release)
# The version number.
set (Tutorial_VERSION_MAJOR 1)
set (Tutorial_VERSION_MINOR 0)

include_directories(../include)

file(GLOB SOURCES "../src/*.cpp")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ../lib)
set (CMAKE_CXX_FLAGS "-std=c++11")

add_library(ConcurrentDataSharer SHARED ${SOURCES})
target_link_libraries(ConcurrentDataSharer  -lboost_serialization -lboost_thread -lboost_system )


install(TARGETS ConcurrentDataSharer DESTINATION //usr/local/share/concurrentDataSharer)