project(kys)
cmake_minimum_required(VERSION 2.6)
cmake_policy(SET CMP0015 OLD)

set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_CXX_COMPILER $ENV{CXX})
set(CMAKE_CXX_FLAGS "-O3 -std=c++14 -pthread -DNDEBUG -DUSE_SDL_MIXER_AUDIO")

include_directories(../include . ../common ../others ../local/include)
link_directories(. ../lib)

file(GLOB SRC_LIST
    ./*.cpp
    ../../common/convert.cpp
    ../../common/File.cpp
    ../others/Hanz2Piny.cpp
    ../others/zip.c)

set(LINK_LIBRARIES SDL2 SDL2_ttf SDL2_image SDL2_mixer SDL2main lua5.3 opencc sqlite3 fmt)
if (APPLE)
    set(LINK_LIBRARIES ${LINK_LIBRARIES} iconv)
endif ()

add_executable(kys ${SRC_LIST})
target_link_libraries(kys ${LINK_LIBRARIES})
