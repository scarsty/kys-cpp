package("smallpot")

    set_homepage("https://github.com/scarsty/smallpot")
    set_description("小水壶播放器：一个轻量级播放器")
    set_license("MIT")

    add_urls("https://github.com/scarsty/smallpot.git")

    add_patches("master", path.join(os.scriptdir(), "patches", "ffmpeg.patch"), "99fdb9358f8a10161e43d9d7c23006f722c8d3f72a56003f686a53650257155d")

    add_deps("libsdl", "libsdl_ttf", "libsdl_image")
    add_deps("ffmpeg")
    add_deps("libass")
    add_deps("mlcc 9a69cfdf86927bd269f8bbc812d68062a976dc73")

    add_defines("_WINDLL")

    on_install(function (package)
       os.cp(path.join(package:scriptdir(), "port", "xmake.lua"), "xmake.lua")
        local configs = {}
        import("package.tools.xmake").install(package, configs)
    end)
