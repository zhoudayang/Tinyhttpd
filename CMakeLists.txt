cmake_minimum_required(VERSION 3.4)
project(Tinyhttpd)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(SOURCE_FILES1
    httpd.c)
set(SOURCE_FILES2
        simpleclient.c)
set(SOURCE_FILES3
        test.cpp
        )
add_executable(httpd ${SOURCE_FILES1})
add_executable(client ${SOURCE_FILES2})
add_executable(okok ${SOURCE_FILES3})
target_link_libraries(client pthread)
target_link_libraries(httpd pthread)
