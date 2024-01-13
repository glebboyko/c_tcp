cmake_minimum_required(VERSION 3.20)
project(c_tcp)

set(CMAKE_CXX_STANDARD 20)

#including project files
include_directories(include)

add_library(${PROJECT_NAME}
        STATIC
        source/tcp-client.cpp source/tcp-server.cpp
        source/basic-ops.cpp source/logger.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "lib_")
