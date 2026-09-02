cmake_minimum_required(VERSION 3.16)

project(concurrent-kv)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(concurrent-kv
    src/main.cpp
    src/store/kv_store.cpp
    src/command/command_processor.cpp
)