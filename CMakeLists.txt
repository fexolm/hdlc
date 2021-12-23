cmake_minimum_required(VERSION 3.2)

project(hdlc)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(main main.cpp)

llvm_map_components_to_libnames(llvm_libs 
    Core
    ExecutionEngine
    Interpreter
    MC
    MCJIT
    Support
    nativecodegen
)
target_link_libraries(main ${llvm_libs})
