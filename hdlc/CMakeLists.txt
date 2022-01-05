add_subdirectory(ast)
add_subdirectory(jit)

add_library(hdlc chip.cpp)
target_link_libraries(hdlc PRIVATE ast jit)
