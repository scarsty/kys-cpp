project(kys)
cmake_minimum_required(VERSION 2.6)

set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_CXX_COMPILER $ENV{CXX})
set(CMAKE_CXX_FLAGS "-O3 -std=c++11 -pthread")

include_directories(../include . ./others)
link_directories(. ../lib)

aux_source_directory(./ SRC_LIST)
aux_source_directory(./others OTHER_SRC_LIST)
list(APPEND SRC_LIST ${OTHER_SRC_LIST})

set(LINK_LIBRARIES SDL2 SDL2_ttf SDL2_image SDL2main lua5.3 bass bassmidi minizip)

add_executable(kys ${SRC_LIST})
target_link_libraries(kys ${LINK_LIBRARIES})
