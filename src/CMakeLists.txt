cmake_minimum_required(VERSION 3.22)

set(KYS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

link_directories("${KYS_ROOT}/local/lib/x64")

find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)
find_package(SDL3_mixer CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(unofficial-lua CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(libzip CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)
find_package(FluidSynth CONFIG REQUIRED)
include(Catch)

set(KYS_COMMON_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${KYS_ROOT}/include"
    "${KYS_ROOT}/mlcc"
    "${KYS_ROOT}/mod"
    "${KYS_ROOT}/others"
    "${KYS_ROOT}/local/include"
)

set(KYS_COMMON_DEFINITIONS WITH_SMALLPOT1 WITH_NETWORK)
if(WIN32)
    list(APPEND KYS_COMMON_DEFINITIONS _WIN32_WINNT=0x0601 WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

file(GLOB KYS_BATTLE_SOURCES CONFIGURE_DEPENDS "battle/*.cpp")

add_library(kys_battle_core STATIC ${KYS_BATTLE_SOURCES})
target_include_directories(kys_battle_core PUBLIC ${KYS_COMMON_INCLUDES})

set(KYS_GAME_SUPPORT_SOURCES
    "${KYS_ROOT}/mlcc/strfunc.cpp"
    "${KYS_ROOT}/mlcc/filefunc.cpp"
    "${KYS_ROOT}/mlcc/ZipFile.cpp"
    "${KYS_ROOT}/mlcc/SQLite3Wrapper.cpp"
    "${KYS_ROOT}/mlcc/SimpleCC.cpp"
    "${KYS_ROOT}/others/Hanz2Piny.cpp"
)

file(GLOB KYS_GAME_LIB_SOURCES CONFIGURE_DEPENDS "*.cpp")
list(REMOVE_ITEM KYS_GAME_LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/kys.cpp")

add_library(kys_game_lib STATIC ${KYS_GAME_LIB_SOURCES} ${KYS_GAME_SUPPORT_SOURCES})
target_include_directories(kys_game_lib PUBLIC ${KYS_COMMON_INCLUDES})
target_compile_definitions(kys_game_lib PUBLIC ${KYS_COMMON_DEFINITIONS})
target_link_libraries(kys_game_lib PUBLIC
    kys_battle_core
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_mixer::SDL3_mixer
    SDL3_ttf::SDL3_ttf
    imgui::imgui
    unofficial::lua::lua
    unofficial::sqlite3::sqlite3
    yaml-cpp::yaml-cpp
    libzip::zip
    FluidSynth::libfluidsynth
    SmallPot
)

set(KYS_PLATFORM_SOURCES)
if(WIN32)
    find_program(KYS_POWERSHELL powershell)
    if(KYS_POWERSHELL)
        add_custom_command(
            OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/kys.ico"
            COMMAND "${KYS_POWERSHELL}" -ExecutionPolicy Bypass -File "${KYS_ROOT}/tools/GenerateAppIcons.ps1" -Target Windows
            DEPENDS "${KYS_ROOT}/assets/app_icon.png" "${KYS_ROOT}/tools/GenerateAppIcons.ps1"
            COMMENT "Generating Windows app icon"
            VERBATIM
        )
        add_custom_target(kys_app_icon DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/kys.ico")
    endif()
    list(APPEND KYS_PLATFORM_SOURCES kys.rc)
endif()

add_executable(kys kys.cpp ${KYS_PLATFORM_SOURCES})
target_link_libraries(kys PRIVATE kys_game_lib)

if(TARGET kys_app_icon)
    add_dependencies(kys kys_app_icon)
endif()

if(MSVC)
    target_compile_options(kys PRIVATE /MP /Zc:preprocessor /source-charset:utf-8 /execution-charset:utf-8)
    target_compile_options(kys_game_lib PRIVATE /MP /Zc:preprocessor /source-charset:utf-8 /execution-charset:utf-8)
    target_compile_options(kys_battle_core PRIVATE /MP /Zc:preprocessor /source-charset:utf-8 /execution-charset:utf-8)
endif()

add_custom_command(TARGET kys POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${KYS_ROOT}/work/game-dev/config"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${KYS_ROOT}/config" "${KYS_ROOT}/work/game-dev/config"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${KYS_ROOT}/docs/更新日志.md" "${KYS_ROOT}/work/game-dev/config/changelog.md"
)

set(KYS_TEST_SOURCES
    "${KYS_ROOT}/tests/TestMain.cpp"
    "${KYS_ROOT}/tests/BattleActionCommitUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleAttackSystemUnitTests.cpp"
    "${KYS_ROOT}/tests/ButtonUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleCastSystemUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessBattleEffectsUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessContextMenuUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessChallengeMenuUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessShopFlowUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessShopRollUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessShopWeightDisplayUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleComboTriggerSystemUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleCoreUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleDamageSystemUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleDeathEffectSystemUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleFrameRunnerRuntimeUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleHitResolverUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleInitializationUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleLogPresenterUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleMovementRealStatsTests.cpp"
    "${KYS_ROOT}/tests/BattlePresentationEffectsUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleProjectileTargetingSystemUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleReportUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleRescueRepositionSystemUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleRuntimeScenarioUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleSceneFrameApplierUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleScenePauseControlUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleSceneRenderMathUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleSceneSetupBuilderUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleSceneUnitStoreUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleTeamEffectSystemUnitTests.cpp"
)

add_executable(kys_tests
    ${KYS_TEST_SOURCES}
)
target_include_directories(kys_tests PRIVATE ${KYS_COMMON_INCLUDES})
target_compile_definitions(kys_tests PRIVATE ${KYS_COMMON_DEFINITIONS})
target_link_libraries(kys_tests PRIVATE kys_game_lib Catch2::Catch2)

if(MSVC)
    target_compile_options(kys_tests PRIVATE /MP /Zc:preprocessor /source-charset:utf-8 /execution-charset:utf-8)
endif()

catch_discover_tests(kys_tests TEST_PREFIX "kys_tests::")
