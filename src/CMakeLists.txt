project(kys)
cmake_minimum_required(VERSION 2.6)

set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_CXX_COMPILER $ENV{CXX})
set(CMAKE_CXX_FLAGS "-O3 -std=c++11 -pthread -DNDEBUG")

include_directories(../include . ../common)
link_directories(. ../lib)

file(GLOB SRC_LIST
    ./*.cpp
    ../common/libconvert.cpp
    ../common/File.cpp
    ../hanzi2pinyin/Hanz2Piny.cpp)

set(LINK_LIBRARIES SDL2 SDL2_ttf SDL2_image SDL2main lua5.3 bass bassmidi minizip opencc)
if (APPLE)
    set(LINK_LIBRARIES ${LINK_LIBRARIES} iconv)
endif ()

add_executable(kys ${SRC_LIST})
target_link_libraries(kys ${LINK_LIBRARIES})
