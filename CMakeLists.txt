cmake_minimum_required(VERSION 3.20)
project(lob CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(tests tests/tests.cpp src/order_book.cpp)
target_include_directories(tests PRIVATE src)

add_executable(bench bench/bench.cpp src/order_book.cpp)
target_include_directories(bench PRIVATE src)
target_compile_options(bench PRIVATE -O2)
