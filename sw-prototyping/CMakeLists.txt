cmake_minimum_required(VERSION 4.1)
project(sw_prototyping C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)

add_executable(sw_prototyping
        src/solver.c
        src/solver.h
        src/driver.cpp
        src/search_driver.cpp
        src/search_driver.hpp
)
