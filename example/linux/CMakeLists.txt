cmake_minimum_required(VERSION 3.5 FATAL_ERROR)  

project(rtsp_test)  

set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

include_directories(../../src)
aux_source_directory(../../src SRCS)

list(APPEND SRCS "server.c" "../simple/media/audio/wave.c")

set(CMAKE_C_FLAGS "${CAMKE_C_FLAGS}  -pthread")

add_executable(rtsp_test ${SRCS})
