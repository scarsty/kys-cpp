cmake_minimum_required(VERSION 3.6)

project(kys_cpp)

set(KYS_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../")
set(KYS_SRC_DIR "${KYS_ROOT_DIR}/src")
set(KYS_MLCC_DIR "${KYS_ROOT_DIR}/mlcc")
set(KYS_OTHERS_DIR "${KYS_ROOT_DIR}/others")
set(KYS_JNI_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../include")
set(KYS_ANDROID_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../lib/${ANDROID_ABI}")
set(VCPKG_INSTALLED_DYNAMIC_DIR "$ENV{VCPKG_ROOT}/installed/arm64-android-dynamic")
set(VCPKG_INSTALLED_ARM64_DIR "$ENV{VCPKG_ROOT}/installed/arm64-android")
set(VCPKG_DYNAMIC_INCLUDE_DIR "${VCPKG_INSTALLED_DYNAMIC_DIR}/include")
set(VCPKG_DYNAMIC_LIB_DIR "${VCPKG_INSTALLED_DYNAMIC_DIR}/lib")

if(NOT EXISTS "${KYS_SRC_DIR}/Application.cpp")
    message(FATAL_ERROR "Invalid source path: ${KYS_SRC_DIR}. Please check CMake relative paths.")
endif()

if(NOT EXISTS "${VCPKG_DYNAMIC_INCLUDE_DIR}")
    message(FATAL_ERROR "Cannot find vcpkg arm64-android-dynamic include directory: ${VCPKG_DYNAMIC_INCLUDE_DIR}. Please set VCPKG_ROOT and install dependencies.")
endif()

set(_sdl_lib_candidates
    "${VCPKG_DYNAMIC_LIB_DIR}"
    "${KYS_ANDROID_LIB_DIR}"
)

set(_sdl_include_candidates
    "${VCPKG_DYNAMIC_INCLUDE_DIR}"
    "${VCPKG_INSTALLED_ARM64_DIR}/include"
)

if(NOT TARGET SDL3::SDL3)
    find_package(SDL3 CONFIG QUIET)
    find_package(SDL3_mixer CONFIG QUIET)
    find_package(SDL3_ttf CONFIG QUIET)
    find_package(SDL3_image CONFIG QUIET)
endif()

if(NOT TARGET SDL3::SDL3)
    set(SDL3_LIBRARY "")
    foreach(_lib_dir IN LISTS _sdl_lib_candidates)
        if(EXISTS "${_lib_dir}/libSDL3.so")
            set(SDL3_LIBRARY "${_lib_dir}/libSDL3.so")
            break()
        endif()
    endforeach()

    set(SDL3_INCLUDE_DIR "")
    foreach(_candidate IN LISTS _sdl_include_candidates)
        if(EXISTS "${_candidate}/SDL3/SDL.h")
            set(SDL3_INCLUDE_DIR "${_candidate}")
            break()
        endif()
        if(EXISTS "${_candidate}/SDL.h")
            set(SDL3_INCLUDE_DIR "${_candidate}")
            break()
        endif()
    endforeach()

    if(SDL3_LIBRARY AND SDL3_INCLUDE_DIR)
        add_library(SDL3::SDL3 UNKNOWN IMPORTED)
        set_property(TARGET SDL3::SDL3 PROPERTY IMPORTED_LOCATION "${SDL3_LIBRARY}")
        set_property(TARGET SDL3::SDL3 PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${SDL3_INCLUDE_DIR}")
    endif()
endif()

if(NOT TARGET SDL3_mixer::SDL3_mixer)
    set(SDL3_MIXER_LIBRARY "")
    foreach(_lib_dir IN LISTS _sdl_lib_candidates)
        if(EXISTS "${_lib_dir}/libSDL3_mixer.so")
            set(SDL3_MIXER_LIBRARY "${_lib_dir}/libSDL3_mixer.so")
            break()
        endif()
    endforeach()

    set(SDL3_MIXER_INCLUDE_DIR "")
    foreach(_candidate IN LISTS _sdl_include_candidates)
        if(EXISTS "${_candidate}/SDL3_mixer/SDL_mixer.h")
            set(SDL3_MIXER_INCLUDE_DIR "${_candidate}")
            break()
        endif()
    endforeach()

    if(SDL3_MIXER_LIBRARY AND SDL3_MIXER_INCLUDE_DIR)
        add_library(SDL3_mixer::SDL3_mixer UNKNOWN IMPORTED)
        set_property(TARGET SDL3_mixer::SDL3_mixer PROPERTY IMPORTED_LOCATION "${SDL3_MIXER_LIBRARY}")
        set_property(TARGET SDL3_mixer::SDL3_mixer PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${SDL3_MIXER_INCLUDE_DIR}")
    endif()
endif()

if(NOT TARGET SDL3_ttf::SDL3_ttf)
    set(SDL3_TTF_LIBRARY "")
    foreach(_lib_dir IN LISTS _sdl_lib_candidates)
        if(EXISTS "${_lib_dir}/libSDL3_ttf.so")
            set(SDL3_TTF_LIBRARY "${_lib_dir}/libSDL3_ttf.so")
            break()
        endif()
    endforeach()

    set(SDL3_TTF_INCLUDE_DIR "")
    foreach(_candidate IN LISTS _sdl_include_candidates)
        if(EXISTS "${_candidate}/SDL3_ttf/SDL_ttf.h")
            set(SDL3_TTF_INCLUDE_DIR "${_candidate}")
            break()
        endif()
    endforeach()

    if(SDL3_TTF_LIBRARY AND SDL3_TTF_INCLUDE_DIR)
        add_library(SDL3_ttf::SDL3_ttf UNKNOWN IMPORTED)
        set_property(TARGET SDL3_ttf::SDL3_ttf PROPERTY IMPORTED_LOCATION "${SDL3_TTF_LIBRARY}")
        set_property(TARGET SDL3_ttf::SDL3_ttf PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${SDL3_TTF_INCLUDE_DIR}")
    endif()
endif()

if(NOT TARGET SDL3_image::SDL3_image)
    set(SDL3_IMAGE_LIBRARY "")
    foreach(_lib_dir IN LISTS _sdl_lib_candidates)
        if(EXISTS "${_lib_dir}/libSDL3_image.so")
            set(SDL3_IMAGE_LIBRARY "${_lib_dir}/libSDL3_image.so")
            break()
        endif()
    endforeach()

    set(SDL3_IMAGE_INCLUDE_DIR "")
    foreach(_candidate IN LISTS _sdl_include_candidates)
        if(EXISTS "${_candidate}/SDL3_image/SDL_image.h")
            set(SDL3_IMAGE_INCLUDE_DIR "${_candidate}")
            break()
        endif()
    endforeach()

    if(SDL3_IMAGE_LIBRARY AND SDL3_IMAGE_INCLUDE_DIR)
        add_library(SDL3_image::SDL3_image UNKNOWN IMPORTED)
        set_property(TARGET SDL3_image::SDL3_image PROPERTY IMPORTED_LOCATION "${SDL3_IMAGE_LIBRARY}")
        set_property(TARGET SDL3_image::SDL3_image PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${SDL3_IMAGE_INCLUDE_DIR}")
    endif()
endif()

if(NOT TARGET SDL3::SDL3)
    message(STATUS "SDL3_LIBRARY='${SDL3_LIBRARY}'")
    message(STATUS "SDL3_INCLUDE_DIR='${SDL3_INCLUDE_DIR}'")
    message(FATAL_ERROR "Cannot find SDL3 or SDL3 headers.\n"
        "Checked libs in: ${_sdl_lib_candidates}\n"
        "Checked headers in: ${_sdl_include_candidates}\n"
        "Please install SDL3 for arm64-android-dynamic via vcpkg.")
endif()

set(CMAKE_CXX_FLAGS "-O2 -std=c++23 -pthread -DNDEBUG")

add_library(main SHARED
        "${KYS_SRC_DIR}/Application.cpp"
        "${KYS_SRC_DIR}/Audio.cpp"
        "${KYS_SRC_DIR}/BattleCursor.cpp"
        "${KYS_SRC_DIR}/BattleMap.cpp"
        "${KYS_SRC_DIR}/BattleMenu.cpp"
        "${KYS_SRC_DIR}/BattleNetwork.cpp"
        "${KYS_SRC_DIR}/BattleScene.cpp"
        "${KYS_SRC_DIR}/BattleSceneAct.cpp"
        "${KYS_SRC_DIR}/BattleSceneHades.cpp"
        "${KYS_SRC_DIR}/BattleScenePaper.cpp"
        "${KYS_SRC_DIR}/BattleSceneSekiro.cpp"
        "${KYS_SRC_DIR}/Button.cpp"
        "${KYS_SRC_DIR}/Camera.cpp"
        "${KYS_SRC_DIR}/ChemistryEngine.cpp"
        "${KYS_SRC_DIR}/Cloud.cpp"
        "${KYS_SRC_DIR}/Console.cpp"
        "${KYS_SRC_DIR}/DrawableOnCall.cpp"
        "${KYS_SRC_DIR}/DrawNode.cpp"
        "${KYS_SRC_DIR}/Engine.cpp"
        "${KYS_SRC_DIR}/Event.cpp"
        "${KYS_SRC_DIR}/Font.cpp"
        "${KYS_SRC_DIR}/GameUtil.cpp"
        "${KYS_SRC_DIR}/GrpIdxFile.cpp"
        "${KYS_SRC_DIR}/Head.cpp"
        "${KYS_SRC_DIR}/InputBox.cpp"
        "${KYS_SRC_DIR}/kys.cpp"
        "${KYS_SRC_DIR}/MainScene.cpp"
        "${KYS_SRC_DIR}/Menu.cpp"
        "${KYS_SRC_DIR}/NewSave.cpp"
        "${KYS_SRC_DIR}/Object.cpp"
        "${KYS_SRC_DIR}/ParticleExample.cpp"
        "${KYS_SRC_DIR}/ParticleSystem.cpp"
        "${KYS_SRC_DIR}/Point.cpp"
        "${KYS_SRC_DIR}/RandomRole.cpp"
        "${KYS_SRC_DIR}/RunNode.cpp"
        "${KYS_SRC_DIR}/RunNodeFromJson.cpp"
        "${KYS_SRC_DIR}/Save.cpp"
        "${KYS_SRC_DIR}/Scene.cpp"
        "${KYS_SRC_DIR}/ScriptLua.cpp"
        "${KYS_SRC_DIR}/ScriptCifa.cpp"
        "${KYS_SRC_DIR}/ShowExp.cpp"
        "${KYS_SRC_DIR}/ShowRoleDifference.cpp"
        "${KYS_SRC_DIR}/SubScene.cpp"
        "${KYS_SRC_DIR}/SuperMenuText.cpp"
        "${KYS_SRC_DIR}/Talk.cpp"
        "${KYS_SRC_DIR}/TeamMenu.cpp"
        "${KYS_SRC_DIR}/TextBox.cpp"
        "${KYS_SRC_DIR}/TextBoxRoll.cpp"
        "${KYS_SRC_DIR}/TextureManager.cpp"
        "${KYS_SRC_DIR}/TitleScene.cpp"
        "${KYS_SRC_DIR}/Types.cpp"
        "${KYS_SRC_DIR}/UI.cpp"
        "${KYS_SRC_DIR}/UIConfig.cpp"
        "${KYS_SRC_DIR}/UIItem.cpp"
        "${KYS_SRC_DIR}/UIKeyConfig.cpp"
        "${KYS_SRC_DIR}/UISave.cpp"
        "${KYS_SRC_DIR}/UIShop.cpp"
        "${KYS_SRC_DIR}/UIStatus.cpp"
        "${KYS_SRC_DIR}/UISystem.cpp"
        "${KYS_SRC_DIR}/Video.cpp"
        "${KYS_SRC_DIR}/VirtualStick.cpp"
        "${KYS_SRC_DIR}/Weather.cpp"
        "${KYS_MLCC_DIR}/ZipFile.cpp"
        "${KYS_MLCC_DIR}/Cifa.cpp"
        "${KYS_MLCC_DIR}/strfunc.cpp"
        "${KYS_MLCC_DIR}/filefunc.cpp"
        "${KYS_MLCC_DIR}/PotConv.cpp"
        "${KYS_OTHERS_DIR}/Hanz2Piny.cpp"
        "${KYS_MLCC_DIR}/SimpleCC.cpp"
        "${KYS_MLCC_DIR}/SQLite3Wrapper.cpp"
)

target_include_directories(main PRIVATE
    "${KYS_SRC_DIR}"
    "${KYS_MLCC_DIR}"
    "${KYS_OTHERS_DIR}"
    "${KYS_JNI_INCLUDE_DIR}"
)

set(_sdl_optional_libs)
if(TARGET SDL3_mixer::SDL3_mixer)
    list(APPEND _sdl_optional_libs SDL3_mixer::SDL3_mixer)
endif()
if(TARGET SDL3_ttf::SDL3_ttf)
    list(APPEND _sdl_optional_libs SDL3_ttf::SDL3_ttf)
endif()
if(TARGET SDL3_image::SDL3_image)
    list(APPEND _sdl_optional_libs SDL3_image::SDL3_image)
endif()

set(_thirdparty_include_candidates
    "${VCPKG_DYNAMIC_INCLUDE_DIR}"
    "${VCPKG_INSTALLED_ARM64_DIR}/include"
)

foreach(_inc IN LISTS _thirdparty_include_candidates)
    if(EXISTS "${_inc}")
        target_include_directories(main PRIVATE "${_inc}")
    endif()
endforeach()

target_link_directories(main PRIVATE "${VCPKG_DYNAMIC_LIB_DIR}" "${KYS_ANDROID_LIB_DIR}")
target_link_libraries(main PRIVATE SDL3::SDL3 sqlite3 lua yaml-cpp zip iconv ${_sdl_optional_libs} m log)
