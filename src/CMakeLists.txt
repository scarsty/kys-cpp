cmake_minimum_required(VERSION 3.16)
project(kys)

set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_CXX_COMPILER $ENV{CXX})
set(CMAKE_CXX_FLAGS "-O2 -std=c++23 -pthread -DNDEBUG")

set(PROJECT_INCLUDE_DIRS
    ../include
    .
    ../mlcc
    ../others
    ../local/include
    ../cifa
    ../glaze/include
    /usr/include
)

set(EXTRA_INCLUDE_DIRS)
set(EXTRA_LINK_DIRS)
set(PROJECT_LINK_DIRS .)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../lib")
    list(APPEND PROJECT_LINK_DIRS ../lib)
endif ()

if (APPLE)
    foreach(prefix /opt/homebrew /opt/homebrew/opt/lua@5.4 /usr/local /usr/local/opt/lua@5.4)
        if (EXISTS "${prefix}")
            list(APPEND CMAKE_PREFIX_PATH "${prefix}")
        endif ()
    endforeach ()

    foreach(dir /opt/homebrew/include /opt/homebrew/opt/lua@5.4/include /usr/local/include /usr/local/opt/lua@5.4/include)
        if (EXISTS "${dir}")
            list(APPEND EXTRA_INCLUDE_DIRS "${dir}")
        endif ()
    endforeach ()

    foreach(dir /opt/homebrew/lib /opt/homebrew/opt/lua@5.4/lib /usr/local/lib /usr/local/opt/lua@5.4/lib)
        if (EXISTS "${dir}")
            list(APPEND EXTRA_LINK_DIRS "${dir}")
        endif ()
    endforeach ()

    if (EXISTS "/opt/homebrew/include")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/opt/homebrew/include")
    endif ()
    if (EXISTS "/usr/local/include")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/usr/local/include")
    endif ()
    if (EXISTS "/opt/homebrew/lib")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/opt/homebrew/lib")
    endif ()
    if (EXISTS "/usr/local/lib")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/lib")
    endif ()
endif ()

include_directories(${PROJECT_INCLUDE_DIRS} ${EXTRA_INCLUDE_DIRS})
link_directories(${PROJECT_LINK_DIRS} ${EXTRA_LINK_DIRS})

file(GLOB SRC_LIST
    ./*.cpp
    ../cifa/Cifa.cpp
    ../mlcc/strfunc.cpp
    ../mlcc/filefunc.cpp
    ../mlcc/PotConv.cpp
    ../mlcc/SimpleCC.cpp
    ../mlcc/SQLite3Wrapper.cpp
    ../mlcc/ZipFile2.cpp
    ../others/Hanz2Piny.cpp    
    )

set(LINK_LIBRARIES SDL3 SDL3_ttf SDL3_image SDL3_mixer lua5.4 sqlite3 yaml-cpp)
if (APPLE)
    set(LINK_LIBRARIES ${LINK_LIBRARIES} iconv)
endif ()

add_executable(kys ${SRC_LIST})
target_link_libraries(kys ${LINK_LIBRARIES})
