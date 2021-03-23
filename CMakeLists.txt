cmake_minimum_required(VERSION 3.16)
project(6502C C)

set(CMAKE_C_STANDARD 11)

add_executable(
        sfotcommon.h
        sfotlib.h
        sfotlib.c
        sfotcommon.c
        sfot.c common.h)