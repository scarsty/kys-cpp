add_rules("mode.debug", "mode.release")

add_repositories("local-repo repo")

set_languages("cxx20")

set_warnings("all", "warning")

if is_plat("windows") then
    add_defines("NOMINMAX")
    add_defines("_CRT_SECURE_NO_WARNINGS")
end

add_requires("libsdl", "libsdl_ttf", "libsdl_image", "libsdl_mixer")
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
        -- add_cxflags("/bigobj")
    end
    add_packages("libsdl", "libsdl_ttf", "libsdl_image", "libsdl_mixer")
    add_packages("lua", "opencc", "sqlite3", "libiconv", "asio", "picosha2", "yaml-cpp", "libzip")
    -- add_linkdirs("local/lib/x64")
    add_defines("USE_SDL_MIXER_AUDIO")
    -- add_links("bass", "bassmidi")
    -- add_defines("WITH_SMALLPOT")
    -- add_packages("smallpot")
