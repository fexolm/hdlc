add_library(jit codegen.cpp module.cpp)
target_link_libraries(jit ${llvm_libs})