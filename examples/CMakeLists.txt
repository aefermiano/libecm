cmake_minimum_required(VERSION 3.10)

project(bin2ecm)
project(ecm2bin)

set(CMAKE_C_FLAGS "-O3")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../bin)

include_directories(../include)

set(STATIC_LIB ${CMAKE_CURRENT_SOURCE_DIR}/../lib/libecm_static.a)

add_executable(bin2ecm bin2ecm.c cmdlinecommon.h cmdlinecommon.c)
add_executable(ecm2bin ecm2bin.c cmdlinecommon.h cmdlinecommon.c)
target_link_libraries(bin2ecm ${STATIC_LIB})
target_link_libraries(ecm2bin ${STATIC_LIB})
