cmake_minimum_required(VERSION 3.20)

project(YaluMart LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ApiServer backend/ApiServer.cpp)

target_include_directories(ApiServer PRIVATE
    "C:/msys64-vigalmart/ucrt64/include"
)

target_link_directories(ApiServer PRIVATE
    "C:/msys64-vigalmart/ucrt64/lib"
)

target_link_libraries(ApiServer PRIVATE
    pq
    ws2_32
)