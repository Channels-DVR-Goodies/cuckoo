cmake_minimum_required(VERSION 3.19)
project(cuckoo C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(cuckoo cuckoo.c)
target_link_libraries( cuckoo config )
