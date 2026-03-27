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

if(EMSCRIPTEN)
    # Keep Dear ImGui from pulling in vcpkg's SDL3 archive: this target already
    # uses Emscripten's threaded SDL port via -sUSE_SDL=3.
    set_property(TARGET imgui::imgui PROPERTY INTERFACE_LINK_LIBRARIES "")
endif()

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
    -O3
    -DNDEBUG
    -pthread
    ${EM_SDL_FLAGS}
)

add_executable(kyschess ${SRC_LIST})

# Link libraries: SDL3_image/SDL3_ttf built from source + vcpkg deps
target_link_libraries(kyschess
    imgui::imgui
    SDL3_mixer
    fluidsynth
    SDL3_image
    webpdemux
    webp
    sharpyuv
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
# Emscripten-specific link flags
set(EM_LINK_FLAGS
    "-O3"
    "${EM_SDL_FLAGS}"
    "-pthread"
    "-sPROXY_TO_PTHREAD"
    "-sWASMFS"
    "-sOFFSCREEN_FRAMEBUFFER"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_MEMORY=268435456"
    "-sFORCE_FILESYSTEM=1"
    "-lfetchfs.js"
    "-sASSERTIONS=1"
    "-fexceptions"
    "--shell-file ${CMAKE_SOURCE_DIR}/shell.html"
    "-sEXPORTED_RUNTIME_METHODS=['ccall']"
)

# Join list into a space-separated string for link flags
list(JOIN EM_LINK_FLAGS " " EM_LINK_FLAGS_STR)
set_target_properties(kyschess PROPERTIES LINK_FLAGS "${EM_LINK_FLAGS_STR}")

# Output .html so emscripten generates the .js and .wasm alongside it
set_target_properties(kyschess PROPERTIES SUFFIX ".html")
