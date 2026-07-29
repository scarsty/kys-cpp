# Third-party licenses

This directory contains license and copyright notices for third-party code,
libraries, and data used by kys-cpp.

## Runtime and build dependencies

The following files are copied verbatim from the corresponding vcpkg package's
`share/<package>/copyright` file:

- `SDL3.txt`, `SDL3_image.txt`, `SDL3_ttf.txt`, `SDL3_mixer.txt`
- `FreeType.txt`, `zlib.txt`, `bzip2.txt`, `libpng.txt`, `Brotli.txt`
- `libwebp.txt`, `mpg123.txt`, `FluidSynth.txt`, `libiconv.txt`, `SQLite3.txt`
- `yaml-cpp.txt`, `Asio.txt`, `Glaze.txt`, `PicoSHA2.txt`

## Embedded code and data

- `Cocos2dx.txt` covers code adapted for the particle system.
- `OpenCC.txt` covers the OpenCC dictionary data distributed in game resources.

## License status requiring clarification

- The `cifa` submodule currently has no explicit upstream license file.
- `others/Hanz2Piny.cpp` and `others/Hanz2Piny.h` are derived from the hanz2piny
  project, whose upstream repository currently has no explicit license file.

Do not assign a license to these components without confirmation from their
copyright holders. Their licensing should be clarified before distributing a
new binary release.