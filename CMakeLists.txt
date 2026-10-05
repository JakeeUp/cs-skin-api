cmake_minimum_required(VERSION 3.20)
project(cs-skin-api CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# MSYS2/MinGW on Windows: let find_package see the ucrt64 packages (curl, nlohmann_json)
if(WIN32 AND EXISTS "C:/msys64/ucrt64")
    list(APPEND CMAKE_PREFIX_PATH "C:/msys64/ucrt64")
    include_directories(C:/msys64/ucrt64/include)
endif()

find_package(CURL REQUIRED)
find_package(nlohmann_json 3 REQUIRED)

# Everything except main.cpp, so tests can link the same code.
add_library(skin_core STATIC
    src/config.cpp
    src/http_client.cpp
    src/optimizer.cpp
    src/steam_market.cpp
    src/validation.cpp
)
target_include_directories(skin_core PUBLIC src third_party)
target_link_libraries(skin_core PUBLIC CURL::libcurl)
target_link_libraries(skin_core PUBLIC nlohmann_json::nlohmann_json)

add_executable(cs-skin-api src/main.cpp)
target_link_libraries(cs-skin-api PRIVATE skin_core)

if(WIN32)
    target_link_libraries(cs-skin-api PRIVATE ws2_32 wsock32 mswsock)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(cs-skin-api PRIVATE Threads::Threads)
endif()

option(BUILD_TESTING "Build the unit tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests/cpp)
endif()
