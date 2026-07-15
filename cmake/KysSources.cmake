function(kys_normalize_root out_var kys_root)
    get_filename_component(kys_root_abs "${kys_root}" REALPATH)
    set(${out_var} "${kys_root_abs}" PARENT_SCOPE)
endfunction()

function(kys_collect_top_level_sources out_var kys_root)
    kys_normalize_root(kys_root_abs "${kys_root}")
    file(GLOB sources CONFIGURE_DEPENDS
        "${kys_root_abs}/src/*.cpp"
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(kys_collect_battle_sources out_var kys_root)
    kys_normalize_root(kys_root_abs "${kys_root}")
    file(GLOB sources CONFIGURE_DEPENDS
        "${kys_root_abs}/src/battle/*.cpp"
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(kys_collect_support_sources out_var kys_root)
    kys_normalize_root(kys_root_abs "${kys_root}")
    set(sources
        "${kys_root_abs}/mlcc/strfunc.cpp"
        "${kys_root_abs}/mlcc/filefunc.cpp"
        "${kys_root_abs}/mlcc/ZipFile.cpp"
        "${kys_root_abs}/mlcc/SQLite3Wrapper.cpp"
        "${kys_root_abs}/mlcc/SimpleCC.cpp"
        "${kys_root_abs}/others/Hanz2Piny.cpp"
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(kys_collect_chess_core_sources out_var kys_root)
    kys_normalize_root(kys_root_abs "${kys_root}")
    set(sources
        "${kys_root_abs}/src/BattlefieldData.cpp"
        "${kys_root_abs}/src/BattleReport.cpp"
        "${kys_root_abs}/src/BattleReportCollector.cpp"
        "${kys_root_abs}/src/BattleSetupFactory.cpp"
        "${kys_root_abs}/src/BattleSummaryBuilder.cpp"
        "${kys_root_abs}/src/HeadlessBattleRunner.cpp"
        "${kys_root_abs}/src/ChessAsciiBoard.cpp"
        "${kys_root_abs}/src/ChessActionOffers.cpp"
        "${kys_root_abs}/src/ChessBalance.cpp"
        "${kys_root_abs}/src/ChessBattleAnalysis.cpp"
        "${kys_root_abs}/src/ChessBattleEffects.cpp"
        "${kys_root_abs}/src/ChessBattleMapCatalog.cpp"
        "${kys_root_abs}/src/ChessBattlePlanner.cpp"
        "${kys_root_abs}/src/ChessBattleText.cpp"
        "${kys_root_abs}/src/ChessCatalogQueries.cpp"
        "${kys_root_abs}/src/ChessCanonicalEncoding.cpp"
        "${kys_root_abs}/src/ChessCliController.cpp"
        "${kys_root_abs}/src/ChessCombo.cpp"
        "${kys_root_abs}/src/ChessContentLoader.cpp"
        "${kys_root_abs}/src/ChessDiagnostics.cpp"
        "${kys_root_abs}/src/ChessEquipment.cpp"
        "${kys_root_abs}/src/ChessGameContent.cpp"
        "${kys_root_abs}/src/ChessGameQueries.cpp"
        "${kys_root_abs}/src/ChessGameSession.cpp"
        "${kys_root_abs}/src/GameVersion.cpp"
        "${kys_root_abs}/src/ChessJsonCodec.cpp"
        "${kys_root_abs}/src/ChessJsonProtocol.cpp"
        "${kys_root_abs}/src/ChessManagementRules.cpp"
        "${kys_root_abs}/src/ChessNeigong.cpp"
        "${kys_root_abs}/src/ChessObservationText.cpp"
        "${kys_root_abs}/src/ChessProgressionRules.cpp"
        "${kys_root_abs}/src/ChessPreparedBattleAnalysis.cpp"
        "${kys_root_abs}/src/ChessReplayArchive.cpp"
        "${kys_root_abs}/src/ChessReplayHash.cpp"
        "${kys_root_abs}/src/ChessReplayJournal.cpp"
        "${kys_root_abs}/src/ChessReplayJson.cpp"
        "${kys_root_abs}/src/ChessReplayVerifier.cpp"
        "${kys_root_abs}/src/ChessRewardRules.cpp"
        "${kys_root_abs}/src/ChessRunRandom.cpp"
        "${kys_root_abs}/src/ChessSaveStore.cpp"
        "${kys_root_abs}/src/ChessSessionCheckpoint.cpp"
        "${kys_root_abs}/src/ChessSessionTypes.cpp"
        "${kys_root_abs}/src/ChessStandaloneBattle.cpp"
        "${kys_root_abs}/src/GrpIdxFile.cpp"
        "${kys_root_abs}/src/InMemZipReader.cpp"
        "${kys_root_abs}/mlcc/filefunc.cpp"
        "${kys_root_abs}/mlcc/SimpleCC.cpp"
        "${kys_root_abs}/mlcc/SQLite3Wrapper.cpp"
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(kys_collect_all_game_sources out_var kys_root)
    kys_collect_top_level_sources(top_level_sources "${kys_root}")
    kys_collect_battle_sources(battle_sources "${kys_root}")
    kys_collect_support_sources(support_sources "${kys_root}")
    set(${out_var}
        ${top_level_sources}
        ${battle_sources}
        ${support_sources}
        PARENT_SCOPE
    )
endfunction()
