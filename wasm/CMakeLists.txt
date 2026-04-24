cmake_minimum_required(VERSION 3.16)
project(kys-wasm)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WASM_DEPS_DIR)
    list(PREPEND CMAKE_PREFIX_PATH "${WASM_DEPS_DIR}")
    set(imgui_DIR "${WASM_DEPS_DIR}/share/imgui")
    set(SDL3_DIR "${WASM_DEPS_DIR}/share/sdl3")
endif()

find_package(imgui CONFIG REQUIRED)

# Source files
file(GLOB SRC_LIST
    ../src/*.cpp
    ../mlcc/strfunc.cpp
    ../mlcc/filefunc.cpp
    ../mlcc/ZipFile.cpp
    ../mlcc/SQLite3Wrapper.cpp
    ../mlcc/SimpleCC.cpp
    ../others/Hanz2Piny.cpp
)

# Include directories
include_directories(
    ../src
    ../include
    ../mlcc
    ../others
    ${WASM_DEPS_DIR}/include
)

# Link directories for pre-built WASM deps (vcpkg or manual)
link_directories(${WASM_DEPS_DIR}/lib)

# Compile flags (SDL3 and zlib come from vcpkg, not Emscripten ports)
add_compile_options(
    -O3
    -DNDEBUG
)

add_executable(kyschess ${SRC_LIST})

# Link libraries: all from vcpkg wasm32-emscripten
target_link_libraries(kyschess
    imgui::imgui
    SDL3_mixer
    fluidsynth
    SDL3_image
    webpdemux
    webpmux
    webp
    sharpyuv
    SDL3_ttf
    freetype
    plutosvg
    plutovg
    png
    brotlidec
    brotlicommon
    yaml-cpp
    zip
    bz2
    sqlite3
    marisa
    SDL3
    z
)
# Emscripten-specific link flags
set(EM_LINK_FLAGS
    "-O3"
    "-sASYNCIFY"
    "-sASYNCIFY_STACK_SIZE=262144"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_MEMORY=268435456"
    "-sFORCE_FILESYSTEM=1"
    "-lidbfs.js"
    "-sASYNCIFY_IMPORTS=__syscall_openat"
    "-sASSERTIONS=1"
    "--shell-file ${CMAKE_SOURCE_DIR}/shell.html"
    "-sEXPORTED_RUNTIME_METHODS=['ccall']"
)

# Join list into a space-separated string for link flags
list(JOIN EM_LINK_FLAGS " " EM_LINK_FLAGS_STR)
set_target_properties(kyschess PROPERTIES LINK_FLAGS "${EM_LINK_FLAGS_STR}")
set_target_properties(kyschess PROPERTIES LINK_DEPENDS "${CMAKE_SOURCE_DIR}/shell.html")

# Output .html so emscripten generates the .js and .wasm alongside it
set_target_properties(kyschess PROPERTIES SUFFIX ".html")
