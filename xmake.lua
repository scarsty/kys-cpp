add_rules("mode.debug", "mode.release")

add_repositories("local-repo repo")

set_languages("cxx20")

set_warnings("all", "warning")

option("audio")
    set_default("sdl")
    set_values("sdl", "bass")
option_end()

option("bass-dir")
    set_default("")
option_end()

option("bassmidi-dir")
    set_default("")
option_end()

if is_plat("windows") then
    add_defines("NOMINMAX")
    add_defines("_CRT_SECURE_NO_WARNINGS")
end

add_requires("libsdl", "libsdl_ttf", "libsdl_image")
if get_config("audio") == "sdl" then
    add_requires("libsdl_mixer")
end
add_requires("lua")
add_requires("opencc")
add_requires("sqlite3")
add_requires("libiconv")
add_requires("asio")
add_requires("yaml-cpp")
add_requires("libzip")
add_requires("picosha2")

-- add_requires("smallpot")

target("kys")
    add_files("src/*.cpp")
    add_files("mlcc/strfunc.cpp")
    add_files("mlcc/filefunc.cpp")
    add_files("mlcc/PotConv.cpp")
    add_files("others/Hanz2Piny.cpp")
    add_includedirs("mlcc", "others", "local/include")
    if is_plat("windows") then
        add_cxflags("/utf-8")
    end
    add_packages("libsdl", "libsdl_ttf", "libsdl_image")
    add_packages("lua", "opencc", "sqlite3", "libiconv", "asio", "picosha2", "yaml-cpp", "libzip")
    if get_config("audio") == "sdl" then
        add_defines("USE_SDL_MIXER_AUDIO")
        add_packages("libsdl_mixer")
    else
        add_links("bass", "bassmidi")
        add_includedirs("$(bass-dir)/c", "$(bassmidi-dir)/c")
        add_linkdirs("$(bass-dir)/c/x64", "$(bassmidi-dir)/c/x64")
        add_runenvs("PATH", "$(bass-dir)/x64", "$(bassmidi-dir)/x64")
    end
    -- add_defines("WITH_SMALLPOT")
    -- add_packages("smallpot")
