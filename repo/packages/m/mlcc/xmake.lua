package("mlcc")

    set_homepage("https://github.com/scarsty/mlcc.git")
    set_description("mlcc utils")
    set_license("Unknown")

    set_urls("https://github.com/scarsty/mlcc.git")

    add_deps("libiconv")

    on_install(function (package)
        os.cp(path.join(package:scriptdir(), "port", "xmake.lua"), "xmake.lua")
        local configs = {}
        import("package.tools.xmake").install(package, configs)
    end)
