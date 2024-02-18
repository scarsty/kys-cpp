add_rules("mode.debug", "mode.release")

add_repositories("local-repo repo")

set_languages("cxx14")

set_warnings("all", "warning")

-- add_requireconfs("*", {build = true})

add_requires("libsdl", "libsdl_ttf", "libsdl_image")
add_requires("ffmpeg")
add_requires("libass")

add_requires("mlcc 9a69cfdf86927bd269f8bbc812d68062a976dc73")

target("libsmallpot")
    set_kind("$(kind)")
    set_basename("smallpot")
    add_files("src/*.cpp|pot.cpp|Engine.cpp")
    add_files("src/others/*.cpp")
    add_includedirs("src/others")
    add_headerfiles("src/PotDll.h")
    if is_plat("windows") then
        add_defines("_WINDLL")
        add_cxflags("/utf-8")
    end
    add_packages("libsdl", "libsdl_ttf", "libsdl_image", "libsdl_mixer")
    add_packages("ffmpeg")
    add_packages("libass")
    add_packages("mlcc")

-- target("smallpot")
--     add_files("src/*.cpp")
--     add_files("src/others/*.cpp")
--     add_includedirs("src/others")
--     if is_plat("windows") then
--         add_cxflags("/utf-8")
--     end
--     add_packages("libsdl", "libsdl_ttf", "libsdl_image", "libsdl_mixer")
--     add_packages("ffmpeg")
--     add_packages("libass")
--     add_packages("mlcc")
