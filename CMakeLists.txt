cmake_minimum_required(VERSION 3.24.2)

project(
    chat
    VERSION 0.1
    DESCRIPTION "Simple chat program using sockets."
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 11)

add_subdirectory(src)
add_subdirectory(apps)
