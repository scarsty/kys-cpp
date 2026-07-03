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
