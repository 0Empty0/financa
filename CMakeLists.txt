cmake_minimum_required(VERSION 3.30)
project(financa C)

set(CMAKE_C_STANDARD 11)

add_executable(financa
        src/main.c
        src/event_loop.c
        src/http_server.c
)
