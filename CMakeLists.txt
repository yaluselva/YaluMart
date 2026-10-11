cmake_minimum_required(VERSION 3.20)

project(YaluMart LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PostgreSQL REQUIRED)

add_executable(ApiServer backend/ApiServer.cpp)

target_link_libraries(ApiServer PRIVATE
    PostgreSQL::PostgreSQL
)