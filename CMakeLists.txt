cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(atomics VERSION 0.1.0)

add_library(atomics INTERFACE)
target_include_directories(atomics INTERFACE include internal)

add_executable(test test.cpp)
target_link_libraries(test PRIVATE atomics)
