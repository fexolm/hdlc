
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(test_parser test_parser.cpp)
target_link_libraries(test_parser gtest_main hdlc)
add_test(NAME test_parser COMMAND test_parser)
target_compile_options(test_parser PRIVATE ${COMPILER_FLAGS})
target_link_options(test_parser PRIVATE ${LINKER_FLAGS})



add_executable(test_chips test_chips.cpp)
target_link_libraries(test_chips gtest_main hdlc)
add_test(NAME test_chips COMMAND test_chips)
target_compile_options(test_chips PRIVATE ${COMPILER_FLAGS})
target_link_options(test_chips PRIVATE ${LINKER_FLAGS})

