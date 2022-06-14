cmake_minimum_required(VERSION 3.22)
project(ex4_OS C)

set(CMAKE_C_STANDARD 99)

add_executable(ex4_OS ex4_client.c ex4_srv.c)
