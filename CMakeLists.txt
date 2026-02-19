cmake_minimum_required(VERSION 3.20)
project(cs-skin-api)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(include)
include_directories(third_party)

add_executable(cs-skin-api src/main.cpp)