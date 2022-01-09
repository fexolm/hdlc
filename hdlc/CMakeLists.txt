add_subdirectory(ast)
add_subdirectory(jit)

add_library(hdlc SHARED chip.cpp)
target_link_libraries(hdlc PRIVATE ast jit)
target_compile_options(hdlc PRIVATE ${COMPILER_FLAGS})
target_link_options(hdlc PRIVATE ${LINKER_FLAGS})
set_target_properties(hdlc PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_OPTS}")
