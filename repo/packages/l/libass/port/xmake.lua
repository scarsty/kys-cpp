add_rules("mode.debug", "mode.release")

set_languages("ansi")

set_warnings("all", "warning")

add_requires("freetype", "fribidi", "harfbuzz")
if is_plat("linux") then
    add_requires("fontconfig")
end

target("ass")
    set_kind("$(kind)")
    add_files("libass/*.c|*ass_coretext.c|*ass_fontconfig.c|*ass_directwrite.c")
    if is_plat("windows") then
        add_files("libass/*ass_directwrite.c")
    elseif is_plat("macosx") then
        add_files("libass/*ass_coretext.c")
    else
        add_files("libass/*ass_fontconfig.c")
    end
    add_headerfiles("libass/ass.h", "libass/ass_types.h", {prefixdir = "ass"})
    add_configfiles("config.h.in")
    set_configdir("$(buildir)/config")
    add_includedirs("$(buildir)/config")
    add_packages("freetype", "fribidi", "harfbuzz")
    if is_plat("linux") then
        add_packages("fontconfig")
    end
