cmake_minimum_required(VERSION 4.2)
project(SolverCoreTestBench C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

add_executable(SolverCoreTestBench
        src/testbench/driver.cpp
        src/testbench/search_driver.cpp
        src/testbench/search_driver.hpp
        src/solver.c
        src/solver.h)
