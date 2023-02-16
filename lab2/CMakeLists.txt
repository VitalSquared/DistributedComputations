cmake_minimum_required(VERSION 3.16)
project(lab2 C)


set(CMAKE_C_STANDARD 99)
set (CMAKE_C_COMPILER "mpicc")
set (CMAKE_CXX_COMPILER "mpic++")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_package(MPI)
include_directories(SYSTEM ${MPI_INCLUDE_PATH})

add_executable(lab2 main.c)
target_link_libraries(lab2 PRIVATE Threads::Threads)
