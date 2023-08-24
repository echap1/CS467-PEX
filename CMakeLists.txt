cmake_minimum_required(VERSION 3.26)
project(PEX C)

set(CMAKE_C_STANDARD 23)

add_executable(server server.c
        common.c
        common.h)
add_executable(client client.c
        common.c
        common.h)
