cmake_minimum_required(VERSION 3.22)
project(kyschess)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# VCPKG_INSTALLED_DIR is passed from build.gradle via -D argument
if(VCPKG_INSTALLED_DIR)
    # NDK toolchain sets CMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY, so we must
    # add vcpkg to CMAKE_FIND_ROOT_PATH (not just CMAKE_PREFIX_PATH) for
    # find_package() to locate configs under share/.
    list(APPEND CMAKE_FIND_ROOT_PATH "${VCPKG_INSTALLED_DIR}")
    list(PREPEND CMAKE_PREFIX_PATH "${VCPKG_INSTALLED_DIR}")
    include_directories("${VCPKG_INSTALLED_DIR}/include")
    link_directories("${VCPKG_INSTALLED_DIR}/lib")
endif()

find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)
find_package(SDL3_mixer CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG QUIET)
if(NOT unofficial-sqlite3_FOUND)
    find_package(SQLite3 REQUIRED)
endif()

# Project root (kys-cpp/)
set(KYS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")

# Source files — mirrors wasm/CMakeLists.txt
file(GLOB KYS_SRC
    "${KYS_ROOT}/src/*.cpp"
    "${KYS_ROOT}/mlcc/strfunc.cpp"
    "${KYS_ROOT}/mlcc/filefunc.cpp"
    "${KYS_ROOT}/mlcc/ZipFile.cpp"
    "${KYS_ROOT}/mlcc/SQLite3Wrapper.cpp"
    "${KYS_ROOT}/mlcc/SimpleCC.cpp"
    "${KYS_ROOT}/others/Hanz2Piny.cpp"
)

add_library(main SHARED ${KYS_SRC})

target_include_directories(main PRIVATE
    "${KYS_ROOT}/src"
    "${KYS_ROOT}/include"
    "${KYS_ROOT}/mlcc"
    "${KYS_ROOT}/others"
)

target_compile_definitions(main PRIVATE NDEBUG)
target_compile_options(main PRIVATE -O2 -fexceptions -frtti)

target_link_libraries(main
    SDL3::SDL3
    SDL3_mixer::SDL3_mixer
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
    imgui::imgui
    yaml-cpp::yaml-cpp
)

if(unofficial-sqlite3_FOUND)
    target_link_libraries(main unofficial::sqlite3::sqlite3)
else()
    target_link_libraries(main SQLite3::SQLite3)
endif()

# Additional static libs from vcpkg
target_link_libraries(main
    fluidsynth
    freetype
    png
    webpdemux webpmux webp sharpyuv
    brotlidec brotlicommon
    zip crypto ssl bz2 z
)

# Android system libs
target_link_libraries(main
    android
    log
    OpenSLES
    EGL
    GLESv3
)
