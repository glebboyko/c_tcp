cmake_minimum_required(VERSION 3.20)
project(c-tcp)

set(CMAKE_CXX_STANDARD 20)

get_filename_component(CTCP_DIR_INCLUDE include ABSOLUTE DIRECTORY)
set(CTCP_DIR_INCLUDE ${CTCP_DIR_INCLUDE} PARENT_SCOPE)

#including project files
include_directories(include source)

add_library(${PROJECT_NAME}
        STATIC
        source/tcp-client.cpp source/tcp-server.cpp
        source/basic-ops.cpp)
add_library(c-tcp_lib ALIAS ${PROJECT_NAME})