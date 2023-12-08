cmake_minimum_required(VERSION 3.20)
project(c_tcp)

set(CMAKE_CXX_STANDARD 20)

get_filename_component(CTCP_DIR_INCLUDE include ABSOLUTE DIRECTORY)
set(${PROJECT_NAME}_DIR ${CTCP_DIR_INCLUDE} PARENT_SCOPE)

#including project files
include_directories(include)

add_library(${PROJECT_NAME}
        STATIC
        source/tcp-client.cpp source/tcp-server.cpp
        source/basic-ops.cpp)
add_library(${PROJECT_NAME}_lib ALIAS ${PROJECT_NAME})
