cmake_minimum_required(VERSION 3.20)
project(cs-skin-api)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(include)
include_directories(third_party)
include_directories(C:/msys64/ucrt64/include)

add_executable(cs-skin-api src/main.cpp)

target_link_libraries(cs-skin-api ws2_32 wsock32 mswsock curl)