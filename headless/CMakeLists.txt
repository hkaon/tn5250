cmake_minimum_required(VERSION 3.12)
project(tn5250-headless LANGUAGES C)

find_package(Threads REQUIRED)

include_directories(${CMAKE_BINARY_DIR} ../lib5250)
add_executable(tn5250-headless headlessterm.c tn5250-headless.c headlessterm.h)
target_link_libraries(tn5250-headless 5250 Threads::Threads)

install(TARGETS tn5250-headless)
