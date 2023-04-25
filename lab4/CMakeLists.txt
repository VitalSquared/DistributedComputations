cmake_minimum_required(VERSION 3.16)
project(lab4 C)

set(CMAKE_C_STANDARD 99)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(version1 version1.c netutils.c netutils.h)

add_executable(version2 version2.c netutils.c netutils.h)

add_executable(version3 version3.c netutils.c netutils.h list.c list.h)
target_link_libraries(version3 PRIVATE Threads::Threads)
