cmake_minimum_required(VERSION 3.20)
project(lob CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(tests tests/tests.cpp src/order_book.cpp src/signals.cpp)
target_include_directories(tests PRIVATE src)

add_executable(bench bench/bench.cpp src/order_book.cpp)
target_include_directories(bench PRIVATE src)
target_compile_options(bench PRIVATE -O2)

# LOBSTER replay + book validation driver
add_executable(obe src/main.cpp src/lobster_parser.cpp src/order_book.cpp)
target_include_directories(obe PRIVATE src)
target_compile_options(obe PRIVATE -O2)

# ITCH replay + invariant checks driver
add_executable(itch src/itch_main.cpp src/itch_parser.cpp src/order_book.cpp)
target_include_directories(itch PRIVATE src)
target_compile_options(itch PRIVATE -O2)

# live crypto feed driver (reads depth snapshots from stdin)
add_executable(crypto src/crypto_main.cpp src/crypto_parser.cpp src/signals.cpp src/order_book.cpp)
target_include_directories(crypto PRIVATE src)
target_compile_options(crypto PRIVATE -O2)

# ITCH replay -> per-event signals CSV (for the Phase-4 analysis)
add_executable(signals src/signals_main.cpp src/itch_parser.cpp src/signals.cpp src/order_book.cpp)
target_include_directories(signals PRIVATE src)
target_compile_options(signals PRIVATE -O2)
