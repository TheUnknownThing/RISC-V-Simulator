cmake_minimum_required(VERSION 3.16)
project(RISCVSimulator VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Include directories
include_directories(src)
include_directories(include)

# Source files
file(GLOB_RECURSE SOURCES 
    "src/core/*.cpp"
    "src/tomasulo/*.cpp" 
    "src/riscv/*.cpp"
    "src/utils/*.cpp"
)

# Main executable
add_executable(riscv_simulator src/main.cpp ${SOURCES})