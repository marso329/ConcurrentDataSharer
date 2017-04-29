cmake_minimum_required (VERSION 3.5.1)
project (pythontest)
set(CMAKE_BUILD_TYPE Release)
# The version number.
set (Tutorial_VERSION_MAJOR 1)
set (Tutorial_VERSION_MINOR 0)

include_directories(../../include /usr/include/python3.5/ )

file(GLOB SOURCES "../../src/*.cpp")
set (CMAKE_CXX_FLAGS "-std=c++11")
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ../../lib/python)

add_library(ConcurrentDataSharerPython SHARED concurrentdatasharer_python.cpp ${SOURCES})
set_target_properties(ConcurrentDataSharerPython PROPERTIES PREFIX "" OUTPUT_NAME ConcurrentDataSharer)
target_link_libraries(ConcurrentDataSharerPython  -lpython3.5m -lboost_python-py35 -lboost_serialization -lboost_thread -lboost_system )