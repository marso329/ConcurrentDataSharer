cmake_minimum_required (VERSION 3.5.1)
project (ConcurrentDataSharer)
set(CMAKE_BUILD_TYPE Release)
# The version number.
set (Tutorial_VERSION_MAJOR 1)
set (Tutorial_VERSION_MINOR 0)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../lib)
set (CMAKE_CXX_FLAGS "-std=c++11")

set(SOURCE
   ${SOURCE}
"${CMAKE_CURRENT_SOURCE_DIR}/../src/concurrentdatasharer.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/../src/structures.cpp" 
"${CMAKE_CURRENT_SOURCE_DIR}/../src/BlockingQueue.cpp"
)

#main library
add_library(ConcurrentDataSharer SHARED ${SOURCE})
target_include_directories(ConcurrentDataSharer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(ConcurrentDataSharer  -lboost_serialization -lboost_thread -lboost_system )
install(TARGETS ConcurrentDataSharer DESTINATION //usr/local/share/concurrentDataSharer)

#python3 library
add_library(ConcurrentDataSharerPython3 SHARED "${CMAKE_CURRENT_SOURCE_DIR}/../src/concurrentdatasharer_python.cpp")
set_target_properties(ConcurrentDataSharerPython3 PROPERTIES PREFIX "" OUTPUT_NAME ConcurrentDataSharerPython3)
target_include_directories(ConcurrentDataSharerPython3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include /usr/include/python3.5/)
target_link_libraries(ConcurrentDataSharerPython3 ConcurrentDataSharer -lpython3.5m -lboost_python-py35 -lboost_serialization -lboost_thread -lboost_system )
install(TARGETS ConcurrentDataSharerPython3 DESTINATION //usr/local/share/concurrentDataSharerPython3)

#python2 library
add_library(ConcurrentDataSharerPython2 SHARED "${CMAKE_CURRENT_SOURCE_DIR}/../src/concurrentdatasharer_python.cpp")
set_target_properties(ConcurrentDataSharerPython2 PROPERTIES PREFIX "" OUTPUT_NAME ConcurrentDataSharerPython2)
target_include_directories(ConcurrentDataSharerPython2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include /usr/include/python2.7/)
target_link_libraries(ConcurrentDataSharerPython2 ConcurrentDataSharer -lpython2.7 -lboost_python-py27 -lboost_serialization -lboost_thread -lboost_system )
install(TARGETS ConcurrentDataSharerPython2 DESTINATION //usr/local/share/concurrentDataSharerPython2)
