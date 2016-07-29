cmake_minimum_required(VERSION 3.5)
project(CreeperDisco)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES main.cpp creeper.h)
add_executable(CreeperDisco ${SOURCE_FILES})

include_directories("lib/curlpp/include")
add_subdirectory(lib/curlpp)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} curlpp pthread)

# Add generated curlpp.dll to the output directory for CreeperDisco target