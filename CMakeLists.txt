cmake_minimum_required(VERSION 2.8)

project(udp-fanout)

add_definitions(-std=c++11)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_LD_FLAGS} -stdlib=libc++")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    message ("GNU Compiler")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_LD_FLAGS} -pthread")
endif()

add_executable(udp-fanout main.cpp net.cpp)
