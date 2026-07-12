cmake_minimum_required(VERSION 3.22)

project(kys-cpp LANGUAGES CXX)

include(CTest)

option(KYS_BUILD_GAME "Build the graphical game" ON)
option(KYS_BUILD_CHESS_CLI "Build the headless auto-chess CLI" ON)

add_subdirectory(src)
