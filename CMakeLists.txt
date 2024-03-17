cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

project(julia_mandelbrot)


add_executable(julia_mandelbrot main.cpp)