cmake_minimum_required(VERSION 3.17)
project(netwalker)

#find_program(CCACHE_FOUND ccache)
#if(CCACHE_FOUND)
#    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
#endif(CCACHE_FOUND)
#INCLUDE_DIRECTORIES("boost/beast")

link_libraries(pthread)
link_libraries(ssl)
link_libraries(crypto)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion -pipe -O3 -DSINGLE_THREAD")

add_executable(netwalker main.cpp server.hpp client.hpp common.hpp log.hpp cipher.h cipher.cpp)