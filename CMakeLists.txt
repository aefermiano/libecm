cmake_minimum_required(VERSION 3.10)

project(ecm)

set(libsrc src/ecm.c include/ecm.h include/common.h src/common.c)

add_library(objlib OBJECT ${libsrc})
include_directories(objlib include)

set_property(TARGET objlib PROPERTY POSITION_INDEPENDENT_CODE 1)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY lib)
set(CMAKE_C_FLAGS "-O3")

add_library(ecm SHARED $<TARGET_OBJECTS:objlib>)
add_library(ecm_static STATIC $<TARGET_OBJECTS:objlib>)

install(TARGETS ecm ecm_static)

add_subdirectory(examples)
