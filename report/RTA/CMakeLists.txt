cmake_minimum_required(VERSION 4.2)
project(RTA CXX)

set(CMAKE_CXX_STANDARD 23)

add_executable(RTA
        Task.hpp
        driver.cpp
        ResponseTimeAnalyser.hpp)
