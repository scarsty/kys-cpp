### 使用到的其他开发库

以下库在Windows下建议使用vcpkg或者msys2来安装（强烈推荐前者，不推荐后者），或者也可以去官网下载，请自行选择。在Linux下编译时则应优先考虑使用系统的包管理器（例如apt等）自动安装的库，在Mac下可以使用homebrew来安装。

- SDL2 <https://www.libsdl.org/>
  - SDL2_image <https://www.libsdl.org/projects/SDL_image/>
  - SDL2_ttf <https://www.libsdl.org/projects/SDL_ttf/>
  - SDL2_mixer <https://www.libsdl.org/projects/SDL_mixer/>
- libiconv <https://www.gnu.org/software/libiconv/>
- lua <https://www.lua.org/>
- PicoSHA2 <https://github.com/okdshin/PicoSHA2>
- sqlite3 <https://www.sqlite.org/>
- OpenCC <https://github.com/BYVoid/OpenCC>
- libzip <https://github.com/nih-at/libzip/>
- asio boost的一部分，需在预处理中打开网络功能，vcpkg可以只安装asio

以下库通常不在包管理工具中，故已包含在工程里。

- hanz2piny <https://github.com/yangyangwithgnu/hanz2piny>
- BASS, BASSMIDI <http://www.un4seen.com/>
- smallpot（动态库版本）: <https://github.com/scarsty/smallpot>
- zip <https://github.com/kuba--/zip> (已不再使用)

以下为间接使用，通常包管理器会自动处理。

注意FFmpeg及以下实际是播放器的依赖。

- zlib <https://zlib.net/>
- freetype <https://www.freetype.org/>
- libpng <http://www.libpng.org/pub/png/libpng.html>
- FFmpeg <https://www.ffmpeg.org/>
- libass <https://github.com/libass/libass>
- fribidi <https://www.fribidi.org/>

- harfbuzz <https://github.com/harfbuzz/harfbuzz>
- fontconfig <https://www.freedesktop.org/wiki/Software/fontconfig/>

PicoSHA2仅需要头文件，如果文件不在包含目录中，请注意将它们复制到适合的位置。

除BASS和BASSMIDI为闭源，但可以免费用于非商业项目之外，其他均为开源工程。

### mlcc

mlcc <https://github.com/scarsty/mlcc>

nb是作者所写的一个通用功能集合，被多个工程使用。

### 视频

参见：<https://github.com/scarsty/smallpot>

这是作者编写的一个视频播放器，可以将其编译为动态库，作为SDL2的插件，用于进行视频过场的播放。

如果难以处理，可以将预处理定义宏中的WITH_SMALLPOT删除。默认不会打开。

### 音频

音频播放可从BASS或者SDL_mixer中二选一，其中BASS的音质较好。

之前SDL_mixer有严重的跳出问题，目前版本是否已经解决暂时不清楚。因BASS为商业库，故使用SDL_mixer作为备选，编译时增加宏USE_SDL_MIXER_AUDIO即可。

链接选项并未分别处理。MSVC和GCC中，如果某个库的功能并未被用到，即使其包含在链接选项中，也不会参与实质的链接。