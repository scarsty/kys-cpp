add_rules("mode.debug", "mode.release")

set_languages("cxx14")

set_warnings("all", "warning")

add_requires("libiconv")

target("mlcc")
    set_kind("static")
    set_kind("$(kind)")
    -- XXX: for smallpot(contains PotConv)
    add_files("*.cpp|PotConv.cpp")
    add_headerfiles("*.h|PotConv.h")
    add_packages("libiconv")
