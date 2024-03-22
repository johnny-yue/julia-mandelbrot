cmake_minimum_required(VERSION 3.21)

set(HIP_PATH "/opt/rocm-6.0.2/" ${HIP_PATH})
set(ROCM_PATH "/opt/rocm-6.0.2/" ${ROCM_PATH})

# wrong config
# set(CMAKE_MODULE_PATH "/opt/rocm-6.0.2/lib/cmake")
# right config
set(CMAKE_MODULE_PATH "/opt/rocm-6.0.2/lib/cmake/hip" ${CMAKE_MODULE_PATH})

message(STATUS "HIP_PATH: ${HIP_PATH}")
message(STATUS "ROCM_PATH: ${ROCM_PATH}")

# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(CMAKE_CXX_COMPILER "/usr/bin/hipcc")
set(CMAKE_CXX_LINKER "/usr/bin/hipcc")

# find_package(png MODULE)
# find_package(X11 MODULE)
# find_package(gl MODULE)
find_package(HIP REQUIRED)
enable_language(HIP)

project(julia_mandelbrot LANGUAGES CXX)

hip_add_executable(julia_mandelbrot main.cpp)

target_link_libraries(julia_mandelbrot PRIVATE png X11 GL pthread fmt)