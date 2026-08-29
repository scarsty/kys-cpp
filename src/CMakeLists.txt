cmake_minimum_required(VERSION 3.22)

set(KYS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
include("${KYS_ROOT}/cmake/KysSources.cmake")
include("${KYS_ROOT}/cmake/KysDeterministicFloatingPoint.cmake")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

link_directories("${KYS_ROOT}/local/lib/x64")

find_package(yaml-cpp CONFIG REQUIRED)
find_package(libzip CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(unordered_dense CONFIG REQUIRED)

set(KYS_BUILD_GRAPHICAL_COMPONENTS OFF)
if(KYS_BUILD_GAME OR BUILD_TESTING)
    set(KYS_BUILD_GRAPHICAL_COMPONENTS ON)
    find_package(SDL3 CONFIG REQUIRED)
    find_package(SDL3_image CONFIG REQUIRED)
    find_package(SDL3_mixer CONFIG REQUIRED)
    find_package(SDL3_ttf CONFIG REQUIRED)
    find_package(imgui CONFIG REQUIRED)
    find_package(unofficial-lua CONFIG REQUIRED)
    find_package(FluidSynth CONFIG REQUIRED)
endif()

if(BUILD_TESTING)
    find_package(Catch2 CONFIG REQUIRED)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    include(Catch)
endif()

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

kys_collect_battle_sources(KYS_BATTLE_SOURCES "${KYS_ROOT}")

add_library(kys_battle_core STATIC ${KYS_BATTLE_SOURCES})
target_include_directories(kys_battle_core PUBLIC ${KYS_COMMON_INCLUDES})
target_compile_definitions(kys_battle_core PRIVATE KYS_HEADLESS_CORE NOMINMAX)
target_link_libraries(kys_battle_core PUBLIC unordered_dense::unordered_dense)
kys_enable_deterministic_floating_point(kys_battle_core)

kys_collect_chess_core_sources(KYS_CHESS_CORE_SOURCES "${KYS_ROOT}")
add_library(kys_chess_core STATIC ${KYS_CHESS_CORE_SOURCES})
target_include_directories(kys_chess_core PUBLIC ${KYS_COMMON_INCLUDES})
target_compile_definitions(kys_chess_core PRIVATE KYS_HEADLESS_CORE NOMINMAX)
target_link_libraries(kys_chess_core PUBLIC
    kys_battle_core
    unofficial::sqlite3::sqlite3
    yaml-cpp::yaml-cpp
    libzip::zip
)
kys_enable_deterministic_floating_point(kys_chess_core)

set(KYS_EFFECT_SCHEMA_OUTPUTS)
if(NOT CMAKE_CROSSCOMPILING AND NOT ANDROID AND NOT EMSCRIPTEN)
    add_executable(kys_effect_schema_codegen
        "${KYS_ROOT}/tools/kys_effect_schema_codegen/main.cpp"
        "${KYS_ROOT}/tools/kys_effect_schema_codegen/ChessEffectSchemaRenderer.cpp"
        "${KYS_ROOT}/tools/kys_effect_schema_codegen/ChessEffectSchemaWriter.cpp"
    )
    target_include_directories(kys_effect_schema_codegen PRIVATE
        ${KYS_COMMON_INCLUDES}
        "${KYS_ROOT}/tools/kys_effect_schema_codegen"
    )
    target_compile_definitions(kys_effect_schema_codegen PRIVATE KYS_HEADLESS_CORE NOMINMAX)
    target_link_libraries(kys_effect_schema_codegen PRIVATE kys_chess_core)
    kys_enable_deterministic_floating_point(kys_effect_schema_codegen)

    set(KYS_EFFECT_SCHEMA_OUTPUTS
        "${KYS_ROOT}/schemas/chess_effects/chess_combos.schema.json"
        "${KYS_ROOT}/schemas/chess_effects/chess_equipment.schema.json"
        "${KYS_ROOT}/schemas/chess_effects/chess_magic_effects.schema.json"
        "${KYS_ROOT}/schemas/chess_effects/chess_neigong.schema.json"
    )
    add_custom_command(
        OUTPUT ${KYS_EFFECT_SCHEMA_OUTPUTS}
        COMMAND $<TARGET_FILE:kys_effect_schema_codegen>
            --output-dir "${KYS_ROOT}/schemas/chess_effects"
        DEPENDS kys_effect_schema_codegen
        COMMENT "Generating chess effect JSON schemas"
        VERBATIM
    )
    set_source_files_properties(${KYS_EFFECT_SCHEMA_OUTPUTS}
        PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
    add_custom_target(kys_effect_schema_outputs_internal
        DEPENDS ${KYS_EFFECT_SCHEMA_OUTPUTS})
    set_property(TARGET kys_effect_schema_outputs_internal PROPERTY FOLDER "Internal")
endif()

if(KYS_BUILD_GRAPHICAL_COMPONENTS)
    kys_collect_support_sources(KYS_GAME_SUPPORT_SOURCES "${KYS_ROOT}")
    kys_collect_top_level_sources(KYS_GAME_LIB_SOURCES "${KYS_ROOT}")
    kys_collect_chess_core_sources(KYS_CHESS_CORE_SOURCES "${KYS_ROOT}")
    list(REMOVE_ITEM KYS_GAME_LIB_SOURCES ${KYS_CHESS_CORE_SOURCES})
    list(REMOVE_ITEM KYS_GAME_SUPPORT_SOURCES
        "${KYS_ROOT}/mlcc/filefunc.cpp"
        "${KYS_ROOT}/mlcc/SimpleCC.cpp"
        "${KYS_ROOT}/mlcc/SQLite3Wrapper.cpp"
    )
    get_filename_component(KYS_ENTRY_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/kys.cpp" REALPATH)
    list(REMOVE_ITEM KYS_GAME_LIB_SOURCES "${KYS_ENTRY_SOURCE}")

    add_library(kys_game_lib STATIC ${KYS_GAME_LIB_SOURCES} ${KYS_GAME_SUPPORT_SOURCES})
    target_include_directories(kys_game_lib PUBLIC ${KYS_COMMON_INCLUDES})
    target_compile_definitions(kys_game_lib PUBLIC ${KYS_COMMON_DEFINITIONS})
    target_link_libraries(kys_game_lib PUBLIC
        kys_battle_core
        kys_chess_core
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
endif()

if(KYS_BUILD_GAME)
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
        list(APPEND KYS_PLATFORM_SOURCES kys.rc kys_utf8.manifest)
    endif()

    add_executable(kys kys.cpp ${KYS_PLATFORM_SOURCES})
    target_link_libraries(kys PRIVATE kys_game_lib)

    if(TARGET kys_app_icon)
        add_dependencies(kys kys_app_icon)
    endif()
endif()

if(KYS_BUILD_CHESS_CLI)
    add_executable(kys_chess_cli "${KYS_ROOT}/cli/ChessCliMain.cpp")
    if(WIN32)
        target_sources(kys_chess_cli PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/kys_utf8.manifest")
    endif()
    target_include_directories(kys_chess_cli PRIVATE ${KYS_COMMON_INCLUDES})
    target_compile_definitions(kys_chess_cli PRIVATE KYS_HEADLESS_CORE NOMINMAX)
    target_link_libraries(kys_chess_cli PRIVATE kys_chess_core kys_battle_core)
    kys_enable_deterministic_floating_point(kys_chess_cli)
endif()

if(MSVC)
    target_compile_options(kys_battle_core PRIVATE /MP /Zc:preprocessor /source-charset:utf-8 /execution-charset:utf-8)
    target_compile_options(kys_chess_core PRIVATE /MP /Zc:preprocessor /source-charset:utf-8 /execution-charset:utf-8)
    if(TARGET kys_effect_schema_codegen)
        target_compile_options(kys_effect_schema_codegen PRIVATE /MP /Zc:preprocessor /source-charset:utf-8 /execution-charset:utf-8)
    endif()
    if(TARGET kys_game_lib)
        target_compile_options(kys_game_lib PRIVATE /MP /Zc:preprocessor /source-charset:utf-8 /execution-charset:utf-8)
    endif()
    if(TARGET kys)
        target_compile_options(kys PRIVATE /MP /Zc:preprocessor /source-charset:utf-8 /execution-charset:utf-8)
    endif()
    if(TARGET kys_chess_cli)
        target_compile_options(kys_chess_cli PRIVATE /MP /Zc:preprocessor /source-charset:utf-8 /execution-charset:utf-8)
    endif()
endif()

if(TARGET kys)
    add_custom_command(TARGET kys POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${KYS_ROOT}/work/game-dev/config"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${KYS_ROOT}/config" "${KYS_ROOT}/work/game-dev/config"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${KYS_ROOT}/docs/更新日志.md" "${KYS_ROOT}/work/game-dev/config/changelog.md"
    )
endif()

if(BUILD_TESTING)
    set(KYS_TEST_SOURCES
    "${KYS_ROOT}/tests/TestMain.cpp"
    "${KYS_ROOT}/tests/BattleActionCommitUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleAttackSystemUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleMathUnitTests.cpp"
    "${KYS_ROOT}/tests/ButtonUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleCastSystemUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleEffectAttackCastSystemUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleEffectEventBridgeUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleEffectSystemUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleEffectCommandSystemUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleRuntimeRandomUnitTests.cpp"
    "${KYS_ROOT}/tests/HeadlessBattleRunnerUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessBattleEffectParserUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessBattleEffectValidationUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessEffectDescriptionUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessMagicEffectDisplayUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessEffectSchemaRendererUnitTests.cpp"
    "${KYS_ROOT}/tools/kys_effect_schema_codegen/ChessEffectSchemaRenderer.cpp"
    "${KYS_ROOT}/tools/kys_effect_schema_codegen/ChessEffectSchemaWriter.cpp"
    "${KYS_ROOT}/tests/ChessBattleAnalysisUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessReplayHashUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessCatalogQueriesUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessContextMenuUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessGameContentUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessGameQueriesUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessGameSessionUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessStandaloneBattleUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessTournamentUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessGuiSessionParityUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessBattlePlannerUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessBattleGuiHeadlessParityUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessProgressionRulesUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessPreparedBattleAnalysisUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessPvpUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessScriptedRunUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessObservationTextUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessSessionCheckpointUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessSystemSettingsMenuUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessJsonCodecUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessJsonProtocolUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessMenuFormattingUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessCliControllerUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessReplayVerifierUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessReplayArchiveUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessReplayGoldenUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessRunRandomUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessEquipmentUnitTests.cpp"
    "${KYS_ROOT}/tests/ChessComboResolutionParityUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleCoreUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleAreaEffectSystemUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleCombatIntentUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleMovementUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleRuntimeRulesUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleRuntimeUnitsUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleStatusSystemUnitTests.cpp"
    "${KYS_ROOT}/tests/FindUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleDamageSystemUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleFrameRunnerRuntimeUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleHitResolverUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleInitializationUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleLogImGuiViewUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleLogPresenterUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleMovementRealStatsTests.cpp"
    "${KYS_ROOT}/tests/BattlePresentationEffectsUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleProjectileTargetingSystemUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleProjectileReflectionPolicyUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleReportUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleRescueRepositionSystemUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleRuntimeScenarioUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleSceneFrameApplierUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleScenePauseControlUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleSceneRenderMathUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleSceneUnitStoreUnitTests.cpp"
    "${KYS_ROOT}/tests/PointerInputUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleSceneControlsUnitTests.cpp"
    "${KYS_ROOT}/tests/PaperCameraControlMathUnitTests.cpp"
    "${KYS_ROOT}/tests/BattleScenePaperCameraTouchUnitTests.cpp"
    )

    add_executable(kys_tests
        ${KYS_TEST_SOURCES}
    )
    target_include_directories(kys_tests PRIVATE
        ${KYS_COMMON_INCLUDES}
        "${KYS_ROOT}/tools/kys_effect_schema_codegen"
    )
    target_compile_definitions(kys_tests PRIVATE ${KYS_COMMON_DEFINITIONS})
    target_link_libraries(kys_tests PRIVATE kys_game_lib Catch2::Catch2)

    if(MSVC)
        target_compile_options(kys_tests PRIVATE /MP /Zc:preprocessor /source-charset:utf-8 /execution-charset:utf-8)
    endif()

    catch_discover_tests(
        kys_tests
        TEST_PREFIX "kys_tests::"
        WORKING_DIRECTORY "${KYS_ROOT}"
    )
    add_test(
        NAME chess_effect_schemas_consumer
        COMMAND "${Python3_EXECUTABLE}"
            "${KYS_ROOT}/tests/test_chess_effect_schemas.py" -v
        WORKING_DIRECTORY "${KYS_ROOT}"
    )
endif()

if(KYS_EFFECT_SCHEMA_OUTPUTS)
    foreach(KYS_EFFECT_SCHEMA_FINAL_TARGET IN ITEMS kys kys_tests kys_chess_cli)
        if(TARGET ${KYS_EFFECT_SCHEMA_FINAL_TARGET})
            add_dependencies(
                ${KYS_EFFECT_SCHEMA_FINAL_TARGET}
                kys_effect_schema_outputs_internal)
        endif()
    endforeach()
endif()
