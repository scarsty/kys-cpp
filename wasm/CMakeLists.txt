cmake_minimum_required(VERSION 3.16)
project(kys-wasm)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
file(GLOB SRC_LIST
    ../src/*.cpp
    ../mlcc/strfunc.cpp
    ../mlcc/filefunc.cpp
    ../mlcc/PotConv.cpp
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

# Emscripten SDL3 port (only SDL3 core is available as a port)
set(EM_SDL_FLAGS
    -sUSE_SDL=3
    -sUSE_ZLIB=1
)

# Compile flags
add_compile_options(
    -O2
    -DNDEBUG
    ${EM_SDL_FLAGS}
)

add_executable(kyschess ${SRC_LIST})

# Link libraries: SDL3_image/SDL3_ttf built from source + vcpkg deps
target_link_libraries(kyschess
    SDL3_mixer
    fluidsynth
    SDL3_image
    SDL3_ttf
    freetype
    plutosvg
    plutovg
    yaml-cpp
    zip
    bz2
    sqlite3
    marisa
)
# "-sJSPI"
# Emscripten-specific link flags
set(EM_LINK_FLAGS
    "${EM_SDL_FLAGS}"
    "-sASYNCIFY"
    "-sASYNCIFY_STACK_SIZE=65536"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_MEMORY=268435456"
    "-sFORCE_FILESYSTEM=1"
    "-lidbfs.js"
    "-sASSERTIONS=1"
    "-fexceptions"
    "--preload-file ${CMAKE_SOURCE_DIR}/../work/game-dev@/game"
    "--shell-file ${CMAKE_SOURCE_DIR}/shell.html"
    "-sEXPORTED_RUNTIME_METHODS=['ccall']"
)

# Join list into a space-separated string for link flags
list(JOIN EM_LINK_FLAGS " " EM_LINK_FLAGS_STR)
set_target_properties(kyschess PROPERTIES LINK_FLAGS "${EM_LINK_FLAGS_STR}")

# Output .html so emscripten generates the .js and .wasm alongside it
set_target_properties(kyschess PROPERTIES SUFFIX ".html")
