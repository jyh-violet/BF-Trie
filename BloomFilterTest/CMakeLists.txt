cmake_minimum_required(VERSION 3.16)
project(Ctest)

set(CMAKE_CXX_STANDARD 14)

add_executable(Ctest
         bloom.h bloom.cpp test.cpp)
