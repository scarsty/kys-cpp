function(kys_enable_deterministic_floating_point target)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE /fp:precise)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
            target_compile_options(${target} PRIVATE
                /clang:-fno-fast-math
                /clang:-ffp-contract=off
            )
        else()
            target_compile_options(${target} PRIVATE
                -fno-fast-math
                -ffp-contract=off
            )
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target} PRIVATE
            -fno-fast-math
            -ffp-contract=off
            -fexcess-precision=standard
        )
    endif()
endfunction()
