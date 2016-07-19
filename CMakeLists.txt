cmake_minimum_required(VERSION 3.5)
project(CreeperDisco)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories("/usr/include")

set(SOURCE_FILES main.cpp creeper.h)
add_executable(CreeperDisco ${SOURCE_FILES})

add_subdirectory(lib/curlpp)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} curlpp curl)

# Edit \usr\include\curl\multi.h to add the following lines
#include <sys/time.h>
#include <sys/select.h>
# Add generated curlpp.dll to the output directory for CreeperDisco target