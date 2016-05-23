cmake_minimum_required(VERSION 3.4)
project(Tinyhttpd)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(SOURCE_FILES1
    httpd.c)

add_executable(httpd ${SOURCE_FILES1})
target_link_libraries(httpd pthread)
