cmake_minimum_required(VERSION 3.2)

project(hdlc)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(hdlc)

add_subdirectory(tests)

add_executable(main main.cpp)

target_link_libraries(main hdlc)