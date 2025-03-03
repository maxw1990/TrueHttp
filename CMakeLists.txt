cmake_minimum_required(VERSION 3.10)

project(HTTpLib)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "-g")

add_executable(HTTpLib first_main.cpp json_parser.h)