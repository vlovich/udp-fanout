cmake_minimum_required(VERSION 2.8)

project(udp-fanout)

add_definitions(-std=c++11)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
	set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} -stdlib=libc++")
endif()

add_executable(udp-fanout main.cpp net.cpp)
