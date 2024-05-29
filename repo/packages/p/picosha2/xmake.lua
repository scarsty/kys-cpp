package("picosha2")

    set_kind("library", {headeronly = true})
    set_homepage("https://github.com/okdshin/PicoSHA2")
    set_description("header-file-only, SHA256 hash generator in C++")
    set_license("MIT")

    add_urls("https://github.com/okdshin/PicoSHA2.git")

    on_install(function (package)
        os.cp("picosha2.h", package:installdir("include"))
    end)
