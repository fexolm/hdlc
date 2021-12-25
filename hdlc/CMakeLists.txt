add_library(hdlc parser.cpp parser_error.cpp)
target_link_libraries(hdlc ${llvm_libs})
