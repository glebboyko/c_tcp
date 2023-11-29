cmake_minimum_required(VERSION 3.20)
project(c-tcp)

set(CMAKE_CXX_STANDARD 20)

#including project files
include_directories(.)


#adding executables
add_executable(c-tcp main.cpp tcp-server.cpp tcp-server.hpp tcp-client.cpp tcp-client.hpp)