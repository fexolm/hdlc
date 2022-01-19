cmake_minimum_required(VERSION 3.2)

project(hdlc)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ENABLE_SANITIZERS "Build with sanitizers enabled" off)
option(ENABLE_CLANG_TIDY "Runs clang tidy in compile time" off)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

find_package(LLVM REQUIRED 12 CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs 
    Core
    ExecutionEngine
    Interpreter
	OrcJit
    Support
    nativecodegen
)

set(COMPILER_FLAGS
    -Wall
    -Werror
    -Wextra
    -Wpedantic
)

if(ENABLE_SANITIZERS)
    list(APPEND COMPILER_FLAGS 
        -fsanitize=address
        -fsanitize=undefined
    )
    list(APPEND LINKER_FLAGS
        -fsanitize=address 
        -fsanitize=undefined
    )
endif()

if(ENABLE_CLANG_TIDY)
    set(CLANG_TIDY_ENABLED_DIAGNOSTICS "performance-*,readability-*,modernize-*,misc-*,cppcoreguidelines-*,clang-analyzer-*,bugprone-*")
    set(CLANG_TIDY_DIRTY_DISABLED_DIAGNOSTICS 
        "-performance-unnecessary-value-param,\
        -modernize-pass-by-value,\
        -modernize-use-trailing-return-type,\
        -readability-named-parameter,\
        -cppcoreguidelines-pro-bounds-pointer-arithmetic,\
        -cppcoreguidelines-pro-type-member-init,\
        -misc-non-private-member-variables-in-classes,\
        -readability-convert-member-functions-to-static,\
        -performance-for-range-copy,\
        -modernize-loop-convert,\
        -cppcoreguidelines-pro-type-cstyle-cast,\
        -performance-no-int-to-ptr,\
        -readability-make-member-function-const,\
        -readability-qualified-auto,\
        -readability-make-member-function-const,\
        -cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers,\
        -bugprone-easily-swappable-parameters,\
        ")

    set(CLANG_TIDY_REAL_DISABLED_DIAGNOSTICS 
        "-cppcoreguidelines-pro-bounds-array-to-pointer-decay,\
        -readability-implicit-bool-conversion,\
        -readability-braces-around-statements,\
        -misc-no-recursion,\
        -cppcoreguidelines-init-variables,\
        ")

    set(CLANG_TIDY_OPTS "clang-tidy;--extra-arg-before=-std=c++17;-checks=${CLANG_TIDY_ENABLED_DIAGNOSTICS},${CLANG_TIDY_DIRTY_DISABLED_DIAGNOSTICS},${CLANG_TIDY_REAL_DISABLED_DIAGNOSTICS}")
endif()

add_subdirectory(hdlc)
add_subdirectory(tests)
