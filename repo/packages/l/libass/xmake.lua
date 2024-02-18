package("libass")

    set_homepage("https://github.com/libass/libass")
    set_description("libass is a portable subtitle renderer for the ASS/SSA subtitle format.")
    set_license("ISC")

    set_urls("https://github.com/libass/libass/archive/$(version).tar.gz",
             "https://github.com/libass/libass.git")

    add_versions("0.16.0", "246091cf034b097dbe362c72170e6996f8d2c19ffe664ce6768ec44eb9ca5c1c")
    add_patches("0.16.0", path.join(os.scriptdir(), "patches", "0.16.0", "fix_inc_fribidi.patch"), "9e777779aa548a9e345dee2782a6eb77cc0ccc8371f6d00c1fac35a1ae367db9")

    add_deps("freetype", "fribidi", "harfbuzz")
    if is_plat("linux") then
        add_deps("fontconfig")
    end

    on_install(function (package)
        os.cp(path.join(package:scriptdir(), "port", "xmake.lua"), "xmake.lua")
        os.cp(path.join(package:scriptdir(), "port", "config.h.in"), "config.h.in")
        local configs = {}
        import("package.tools.xmake").install(package, configs)
    end)
