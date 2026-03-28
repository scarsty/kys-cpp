vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO FluidSynth/fluidsynth
    REF "v${VERSION}"
    SHA512 5c376d9bf6388f04e5d48375a70682f9d7edcd65809383afc0190c77140b086492abc17a8d3a2aa07e59dde681ab17a919e9b8b7e174a91a2951c30b262c10e4
    HEAD_REF master
    PATCHES
        fix-gcem.patch
        cxx-linkage-pkgconfig.diff
        emscripten-no-pthread.patch
        skip-fake-filename-stat.patch
)
# Do not use or install FindSndFileLegacy.cmake and its deps
file(REMOVE
    "${SOURCE_PATH}/cmake_admin/FindFLAC.cmake"
    "${SOURCE_PATH}/cmake_admin/Findmp3lame.cmake"
    "${SOURCE_PATH}/cmake_admin/Findmpg123.cmake"
    "${SOURCE_PATH}/cmake_admin/FindOgg.cmake"
    "${SOURCE_PATH}/cmake_admin/FindOpus.cmake"
    "${SOURCE_PATH}/cmake_admin/FindSndFileLegacy.cmake"
    "${SOURCE_PATH}/cmake_admin/FindVorbis.cmake"
    "${SOURCE_PATH}/cmake_admin/FindGCEM.cmake"
)

vcpkg_check_features(
    OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        libinstpatch enable-libinstpatch
        sndfile      enable-libsndfile
        pulseaudio   enable-pulseaudio
)

# enable platform-specific features, force the build to fail if the required libraries are not found,
# and disable all other features to avoid system libraries to be picked up
set(WINDOWS_OPTIONS enable-dsound enable-wasapi enable-waveout enable-winmidi HAVE_MMSYSTEM_H HAVE_DSOUND_H HAVE_OBJBASE_H)
set(MACOS_OPTIONS enable-coreaudio enable-coremidi COREAUDIO_FOUND COREMIDI_FOUND)
set(LINUX_OPTIONS enable-alsa ALSA_FOUND)
set(ANDROID_OPTIONS enable-opensles OpenSLES_FOUND)
set(IGNORED_OPTIONS enable-coverage enable-dbus enable-floats enable-fpe-check enable-framework enable-jack
    enable-libinstpatch enable-midishare enable-oboe enable-openmp enable-oss enable-pipewire enable-portaudio
    enable-profiling enable-readline enable-sdl3 enable-systemd enable-trap-on-fpe enable-ubsan)

if(VCPKG_TARGET_IS_WINDOWS)
    set(OPTIONS_TO_ENABLE ${WINDOWS_OPTIONS})
    set(OPTIONS_TO_DISABLE ${MACOS_OPTIONS} ${LINUX_OPTIONS} ${ANDROID_OPTIONS})
elseif(VCPKG_TARGET_IS_OSX)
    set(OPTIONS_TO_ENABLE ${MACOS_OPTIONS})
    set(OPTIONS_TO_DISABLE ${WINDOWS_OPTIONS} ${LINUX_OPTIONS} ${ANDROID_OPTIONS})
elseif(VCPKG_TARGET_IS_LINUX)
    set(OPTIONS_TO_ENABLE ${LINUX_OPTIONS})
    set(OPTIONS_TO_DISABLE ${WINDOWS_OPTIONS} ${MACOS_OPTIONS} ${ANDROID_OPTIONS})
elseif(VCPKG_TARGET_IS_ANDROID)
    set(OPTIONS_TO_ENABLE ${ANDROID_OPTIONS})
    set(OPTIONS_TO_DISABLE ${WINDOWS_OPTIONS} ${MACOS_OPTIONS} ${LINUX_OPTIONS})
else()
    # Emscripten / other: disable all platform audio drivers
    set(OPTIONS_TO_DISABLE ${WINDOWS_OPTIONS} ${MACOS_OPTIONS} ${LINUX_OPTIONS} ${ANDROID_OPTIONS})
endif()

foreach(_option IN LISTS OPTIONS_TO_ENABLE)
    list(APPEND ENABLED_OPTIONS "-D${_option}:BOOL=ON")
endforeach()

foreach(_option IN LISTS OPTIONS_TO_DISABLE IGNORED_OPTIONS)
    list(APPEND DISABLED_OPTIONS "-D${_option}:BOOL=OFF")
endforeach()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        ${ENABLED_OPTIONS}
        ${DISABLED_OPTIONS}
        "-Dosal=cpp11"
        -DCMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON
    MAYBE_UNUSED_VARIABLES
        ${OPTIONS_TO_DISABLE}
        enable-coverage
        enable-framework
        enable-ubsan
)

if(VCPKG_TARGET_IS_EMSCRIPTEN)
    # On Emscripten the CLI tool cannot link (missing pthread symbols).
    # Build only the library, then do a manual install.
    vcpkg_cmake_build(TARGET libfluidsynth)

    # Install library
    file(GLOB _dbg_lib "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-dbg/src/libfluidsynth.a")
    file(GLOB _rel_lib "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/src/libfluidsynth.a")
    if(_rel_lib)
        file(INSTALL ${_rel_lib} DESTINATION "${CURRENT_PACKAGES_DIR}/lib")
    endif()
    if(_dbg_lib)
        file(INSTALL ${_dbg_lib} DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib")
    endif()

    # Install headers: source headers + generated fluidsynth.h and version.h
    file(INSTALL "${SOURCE_PATH}/include/fluidsynth/" DESTINATION "${CURRENT_PACKAGES_DIR}/include/fluidsynth"
         FILES_MATCHING PATTERN "*.h")
    # Generated headers are in the build dir
    file(GLOB _gen_h "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/include/fluidsynth.h")
    if(_gen_h)
        file(INSTALL ${_gen_h} DESTINATION "${CURRENT_PACKAGES_DIR}/include")
    endif()
    file(GLOB _version_h "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/include/fluidsynth/version.h")
    if(_version_h)
        file(INSTALL ${_version_h} DESTINATION "${CURRENT_PACKAGES_DIR}/include/fluidsynth")
    endif()

    # Install cmake config
    if(EXISTS "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/FluidSynthConfig.cmake")
        file(INSTALL "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/FluidSynthConfig.cmake"
             DESTINATION "${CURRENT_PACKAGES_DIR}/share/fluidsynth")
    endif()

    # Generate a minimal pkgconfig file so downstream ports (SDL3_mixer) pass validation.
    file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/lib/pkgconfig")
    file(WRITE "${CURRENT_PACKAGES_DIR}/lib/pkgconfig/fluidsynth.pc"
"prefix=\${pcfiledir}/../..
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: FluidSynth
Description: Software synthesizer based on the SoundFont 2 spec
Version: ${VERSION}
Libs: -L\${libdir} -lfluidsynth
Cflags: -I\${includedir}
")
    file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig")
    file(WRITE "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig/fluidsynth.pc"
"prefix=\${pcfiledir}/../../..
libdir=\${prefix}/debug/lib
includedir=\${prefix}/include

Name: FluidSynth
Description: Software synthesizer based on the SoundFont 2 spec
Version: ${VERSION}
Libs: -L\${libdir} -lfluidsynth
Cflags: -I\${includedir}
")
else()
    vcpkg_cmake_install()
    vcpkg_copy_pdbs()
    vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/fluidsynth)
    vcpkg_fixup_pkgconfig()
    vcpkg_copy_tools(TOOL_NAMES fluidsynth AUTO_CLEAN)
endif()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
    "${CURRENT_PACKAGES_DIR}/share/man")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

