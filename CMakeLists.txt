cmake_minimum_required(VERSION 3.17)
project(netwalker)

#INCLUDE_DIRECTORIES("boost/beast")

link_libraries(pthread)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -O2 -Wall -Wextra -Wconversion")

add_executable(netwalker main.cpp server.hpp client.hpp common.hpp log.hpp cipher.h cipher.cpp)