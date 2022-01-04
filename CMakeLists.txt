cmake_minimum_required(VERSION 3.2)

project(hdlc)

include(CTest)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs 
    Core
    ExecutionEngine
    Interpreter
	OrcJit
    Support
    nativecodegen
)

add_subdirectory(hdlc)

add_subdirectory(tests)

add_executable(main main.cpp)

target_link_libraries(main hdlc)
