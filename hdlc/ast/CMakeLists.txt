add_library(ast STATIC parser.cpp parser_error.cpp ast.cpp casts.cpp)
target_compile_options(ast PRIVATE ${COMPILER_FLAGS})
target_link_options(ast PRIVATE ${LINKER_FLAGS})
set_target_properties(ast PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_OPTS}")
