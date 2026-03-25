cmake_minimum_required(VERSION 3.20)
project(cs-skin-api)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(include)
include_directories(third_party)

# MSYS2/MinGW on Windows: add ucrt64 include path if present
if(WIN32 AND EXISTS "C:/msys64/ucrt64/include")
    include_directories(C:/msys64/ucrt64/include)
endif()

find_package(CURL REQUIRED)

add_executable(cs-skin-api src/main.cpp)

if(WIN32)
    target_link_libraries(cs-skin-api ws2_32 wsock32 mswsock CURL::libcurl)
else()
    target_link_libraries(cs-skin-api CURL::libcurl pthread)
endif()
