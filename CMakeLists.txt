cmake_minimum_required(VERSION 3.16)
project(cuckoo C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(cuckoo cuckoo.c)

target_link_libraries( cuckoo asan )

target_compile_options( cuckoo PUBLIC "-fsanitize=address" )
target_compile_options( cuckoo PUBLIC "-fstack-protector" )
target_compile_options( cuckoo PUBLIC "-fno-omit-frame-pointer")

install( TARGETS cuckoo RUNTIME
         DESTINATION /usr/bin )
