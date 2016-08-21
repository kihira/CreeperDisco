cmake_minimum_required(VERSION 3.5)
project(CreeperDisco)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES main.cpp creeper.h discord.h)
add_executable(CreeperDisco ${SOURCE_FILES})

# Boost
# log requires filesystem and thread
# asio requires system
find_package(Boost REQUIRED COMPONENTS system log filesystem thread)
include_directories(${Boost_INCLUDE_DIRS})

# Other libs
include_directories("lib/curlpp/include")
include_directories("lib/websocketpp")
add_subdirectory(lib/curlpp)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${Boost_LIBRARIES} curlpp pthread ssl crypto)
add_definitions(-DBOOST_LOG_DYN_LINK) # required for log

# Add generated curlpp.dll to the output directory for CreeperDisco target