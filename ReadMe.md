# kys-cpp

金庸群侠传复刻版，为区别于其他语言的复刻版，添加后缀cpp。

<img src='https://raw.githubusercontent.com/scarsty/kys-cpp/master/logo.png' />

目前的开发分支名为“黑帝斯”，即受2020~2021年大火游戏《黑帝斯》启发的即时战斗系统。

在ini中设置semi_real=2即可以开启此战斗系统，但是并不正常。之前的回合制以及半即时战斗系统不受影响。

github：https://github.com/scarsty/kys-cpp

码云（不定期同步）：https://gitee.com/scarsty/kys-cpp

资源文件：<http://pan.baidu.com/s/1sl2X9wD>

这是一个以SDL2为基础实现的2D游戏框架，同时相当于提供了一个使用该框架制作DOS游戏《金庸群侠传》移植版的范例。

Windows下可以使用Visual Studio编译，其他系统下可以在src目录使用CMake生成Makefile，使用GCC或Clang编译，需至少支持C++14。

VS工程为x64版本，如需要x86版请自行修改。

## 使用到的其他开发库

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
- asio boost的一部分，需在预处理中打开网络功能，vcpkg可以只安装asio

以下库通常不在包管理工具中，故已包含在工程里。

- hanz2piny <https://github.com/yangyangwithgnu/hanz2piny>
- zip <https://github.com/kuba--/zip>
- BASS, BASSMIDI <http://www.un4seen.com/>
- smallpot（动态库版本）: <https://github.com/scarsty/smallpot>

以下为间接使用，通常包管理器会自动处理。

- freetype <https://www.freetype.org/>
- FFmpeg <https://www.ffmpeg.org/>
- zlib <https://zlib.net/>
- libass <https://github.com/libass/libass>
- fribidi <https://www.fribidi.org/>
- libpng <http://www.libpng.org/pub/png/libpng.html>
- harfbuzz <https://github.com/harfbuzz/harfbuzz>
- fontconfig <https://www.freedesktop.org/wiki/Software/fontconfig/>

PicoSHA2和CSV库仅需要头文件，如果文件不在包含目录中，请注意将它们复制到适合的位置。

除BASS和BASSMIDI为闭源，但可以免费用于非商业项目之外，其他均为开源工程。

### nb

nb <https://github.com/scarsty/nb>

nb是作者所写的一个通用功能集合，被多个工程使用。

其中包含了ini文件读写库，修改自以下工程：

- ini Reader <https://github.com/benhoyt/inih>

可以用以下命令获取：

```shell
git clone https://github.com/scarsty/nb
```

nb需与kys-cpp目录同级。

### 视频

参见：<https://github.com/scarsty/smallpot>

这是作者编写的一个视频播放器，可以将其编译为动态库，作为SDL2的插件，用于进行视频过场的播放。

如果难以处理，可以将预处理定义宏中的WITH_SMALLPOT删除。Mac和Linux下默认不会打开。

### 音频

音频播放可从BASS或者SDL_mixer中二选一，其中BASS的音质较好。

之前SDL_mixer有严重的跳出问题，目前版本是否已经解决暂时不清楚。因BASS为商业库，故使用SDL_mixer作为备选，编译时增加宏USE_SDL_MIXER_AUDIO即可。

链接选项并未分别处理。MSVC和GCC中，如果某个库的功能并未被用到，即使其包含在链接选项中，也不会参与实质的链接。

## 授权

以下文本，若中文和英文存在冲突，则以中文为准。

```
以 BSD 3-Clause License 授权发布，但是包含两个附加条款：
一般情况下，可以自由使用代码，也可自由用于商业情况。
但若将其用于金庸武侠题材的游戏，则严禁任何形式的牟利行为。

The source codes are distributed under BSD 3-Clause License license, with two additional clauses.
Full right of the codes is granted.
If the codes are used in Jin Yong's novels related games, the game is strictly prohibited for profit.
```

## 运行截图

<img src='https://raw.githubusercontent.com/scarsty/kys-cpp/master/pic/1.png' />

<img src='https://raw.githubusercontent.com/scarsty/kys-cpp/master/pic/2.png' />

<img src='https://raw.githubusercontent.com/scarsty/kys-cpp/master/pic/3.png' />


## 其他

Special thanks to ReSharper C++ for its support to the open source community.

<img src='https://resources.jetbrains.com/storage/products/company/brand/logos/ReSharperCPP_icon.svg'>

Special thanks to WangZi, NiBa, HuaKaiYeLuo, XiaoWu, LiuYunFeiYue, ZhenZhengDeQiangQiang, SanDaShan, SB250 and SB750.

纪念金庸先生对武侠文化的贡献。

