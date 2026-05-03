cmake_minimum_required(VERSION 3.16)
project(kys)
cmake_policy(SET CMP0015 OLD)

set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_CXX_COMPILER $ENV{CXX})
set(CMAKE_CXX_FLAGS "-O2 -std=c++23 -pthread -DNDEBUG")
find_package(imgui CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)
include_directories(../include . ../mlcc ../others ../local/include /usr/include)
link_directories(. ../lib)

file(GLOB SRC_LIST
    ./*.cpp
    ../mlcc/strfunc.cpp
    ../mlcc/filefunc.cpp
    ../others/Hanz2Piny.cpp
    )
set(LINK_LIBRARIES SDL3 SDL3_ttf SDL3_image SDL3_mixer lua5.4 opencc sqlite3 yaml-cpp zip)

add_library(kys_battle_core
    battle/BattleAttackSystem.cpp
    battle/BattleComboTriggerSystem.cpp
    battle/BattleCore.cpp
    battle/BattleCombatIntent.cpp
    battle/BattleDamageSystem.cpp
    battle/BattleDeathEffectSystem.cpp
    battle/BattleEffectSystem.cpp
    battle/BattleGeometry.cpp
    battle/BattleMovement.cpp
    battle/BattlePresentation.cpp
    battle/BattlePresentationPlayback.cpp
    battle/BattleProjectileTargetingSystem.cpp
    battle/BattleStatusSystem.cpp
    battle/BattleTeamEffectSystem.cpp
)
target_include_directories(kys_battle_core PUBLIC .)

add_executable(kys ${SRC_LIST})
target_link_libraries(kys imgui::imgui kys_battle_core ${LINK_LIBRARIES})

add_executable(kys_tests
    ../tests/BattleAttackSystemUnitTests.cpp
    ../tests/BattleComboTriggerSystemUnitTests.cpp
    ../tests/BattleCoreUnitTests.cpp
    ../tests/BattleDamageSystemUnitTests.cpp
    ../tests/BattleDeathEffectSystemUnitTests.cpp
    ../tests/BattleEffectSystemUnitTests.cpp
    ../tests/BattleMovementRealStatsTests.cpp
    ../tests/BattlePresentationPlaybackUnitTests.cpp
    ../tests/BattlePresentationUnitTests.cpp
    ../tests/BattleProjectileTargetingSystemUnitTests.cpp
    ../tests/BattleTeamEffectSystemUnitTests.cpp
)
target_include_directories(kys_tests PRIVATE . ../mlcc ../mod ../local/include)
target_link_libraries(kys_tests kys_battle_core Catch2::Catch2WithMain)
enable_testing()
add_test(NAME kys_tests COMMAND kys_tests)
