# 金庸老先生千古！

# kys-cpp

<img src='https://raw.githubusercontent.com/scarsty/kys-cpp/master/logo.png' />

github：https://github.com/scarsty/kys-cpp

码云（不定期同步）：https://gitee.com/scarsty/kys-cpp

资源文件：<http://pan.baidu.com/s/1sl2X9wD>

这是一个以SDL2为基础实现的2D游戏框架，同时相当于提供了一个使用该框架制作DOS游戏《金庸群侠传》移植版的范例。

Windows下可以使用Visual Studio编译，其他系统下可以在src目录使用CMake生成Makefile，使用GCC或Clang编译，需至少支持C++14。

VS工程为x64版本，如需要x86版请自行修改。

## 架构的简单说明

### 公共部分

Engine封装了一套SDL2的主要实现，与SmallPot类似。如更换绘图引擎，则只需修改此部分即可。

Save中对所有数据进行了封装，可以较为方便地调用。

TextureManger是一个纹理管理器，因为《金庸群侠传》的贴图是含偏移设置的，故有些特殊的地方。

Audio是音频类，基于BASS或者SDL_mixer，可以播放mid、mp3、wav等格式。

PotConv封装了iconv的实现。

### RunNode

RunNode是游戏中的基本执行类，包含5个重要的虚函数：backRun，draw，dealEvent，onEnter，onExit。对应在背景中执行，如何画自身，如何处理事件，进入时的处理，退出时的处理。一般来说，衍生类应重写这些函数。

其中每个节点可以包含数个子节点，在绘图时子节点也会被自动一一绘出。注意在画自身的部分不需要处理子节点，除非有特殊的需要。

存在一个全局的的RunNode栈root（实际是std::vector），会从下到上依次画出每个节点。RunNode类有一个占满全屏的属性，表示这个类将占用全部的屏幕，因此引擎在绘制的时候，会仅找出最靠上的含有该属性的节点，并从这里开始往上画。

创建一个节点，并调用run过程即可运行此节点，注意使用run执行的节点是完全独占的，其子节点也会有事件响应。如果需要退出当前节点，在适当的地方使用setExit(true)即可，但是子节点调用是无效的，除非拥有当前运行节点的指针。

run过程的参数为一个布尔值，如果为true则会被加入到root并进行绘制，如果为false则只运行不参与绘制。但是很多节点的draw过程是空的，即使放到root中也不会参与绘制，实际利用了这一特性的仅有显示人物对话的部分。

run过程会返回一个函数值，可以利用进行一些判断，例如菜单的选择。

规定所有节点均使用共享指针，可以比较自由地互相包含。请不要让子节点出现递归包含，这样会迅速消耗掉所有资源。

通常来说，大部分游戏引擎都需要全局标记和回调来控制剧情的执行，本框架的设计在绘图无阻塞执行的同时，事件仍是以阻塞的模式顺序执行的，这样无需额外的事件标记。

更多分析见<https://www.dawuxia.net/thread-1039649-1-1.html>。

### 视频

参见：<https://github.com/scarsty/smallpot>

这是作者编写的一个视频播放器，可以将其编译为动态库，作为SDL2的插件，用于进行视频过场的播放。

如果难以处理，可以将预处理定义宏中的WITH_SMALLPOT删除。Mac和Linux下默认不会打开。

### 音频

音频播放可从BASS或者SDL_mixer中二选一，其中BASS的音质较好。

之前SDL_mixer有严重的跳出问题，目前版本是否已经解决暂时不清楚。因BASS为商业库，故使用SDL_mixer作为备选，编译时增加宏USE_SDL_MIXER_AUDIO即可。

链接选项并未分别处理。VS和GCC中，如果某个库的功能并未被用到，即使其包含在链接选项中，也不会参与实质的链接。

## 资源的保存以及abc工程

存档的基础数据部分（即r部分）可以保存为sqlite的数据库格式。可以通过读取和保存来转换已有存档。

游戏的资源文件是以单个图片的形式放在resource的各个目录中的，每张图的偏移保存在index.ka中，格式为每张图两个16位整数，连续存放。这种类型的目录也可以打包为zip格式。

战斗贴图文件中，每个人的帧数，之前在hugebase（水浒）框架中使用fightframe.ka保存，现改用fightframe.txt保存。格式为动作索引（0~4），每方向数量。未写则视为0。这种类型的目录也可以打包为zip格式。

之前游戏使用的列表文件只保留了升级经验列表和离队列表，改用txt格式。

存档文件的文本编码，仅有初始存档为cp950（BIG5），这是向下兼容的需要，但是内部会使用65001（utf-8），存档被保存后也会转为65001。存档中有一个标记位保存了该文件的编码。

abc工程用来转换之前的数据。建议自行调整代码后，使用调试模式执行。

其中主要的功能是将存档的R部分扩展为原来的二倍。即所有的16位整数转为32位整数，表示范围从32767扩大到2^31-1，足够通常的数值使用。同时，原有的字串也扩展为之前的二倍长度，例如原来人物的名字有5个中文字符长度，实际上最多只能使用4个字，转换之后因字符串的编码使用了utf-8，可以使用6个汉字（并不是推荐你用6个字）。但是如果使用数据库来存档，则长度不受此限制。转换之后的文件名变为r?.grp32。

并非所有的文档都转为32位，部分是为了节省资源的需要，以及某些情况过于复杂。

以上提到的数据，除了文本文件外均可以用真正的强强的新版upedit修改（该修改器不完善）。

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
- {fmt} <https://github.com/fmtlib/fmt>
- asio boost的一部分，需在预处理中打开网络功能，vcpkg可以只安装asio

以下库通常不在包管理工具中，故已包含在工程里。

- hanz2piny <https://github.com/yangyangwithgnu/hanz2piny>
- zip <https://github.com/kuba--/zip>
- BASS, BASSMIDI <http://www.un4seen.com/>
- Fast C++ CSV Parser: <https://github.com/ben-strasser/fast-cpp-csv-parser>
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

{fmt}即将进入C++20成为std::format，届时可以简单修改后，不使用外部库。

PicoSHA2和CSV库仅需要头文件，如果文件不在包含目录中，请注意将它们复制到适合的位置。

除BASS和BASSMIDI为闭源，但可以免费用于非商业项目之外，其他均为开源工程。

### common

common <https://github.com/scarsty/common>

common是作者所写的一个通用功能集合，被多个工程使用。

其中包含了ini文件读写库，修改自以下工程：

- ini Reader <https://github.com/benhoyt/inih>

可以用以下命令获取：

```shell
git clone https://github.com/scarsty/common common
```

common需与kys-cpp目录同级。

## 授权

以下文本，若中文和英文存在冲突，则以中文为准。

```
以zlib授权发布，但是包含两个附加条款：
一般情况下，可以自由使用代码，也可自由用于商业情况。
但若将其用于金庸武侠题材的游戏，则严禁任何形式的牟利行为。

The source codes are distributed under zlib license, with two additional clauses.
Full right of the codes is granted.
If the codes are used in Jin Yong's novels related games, the game is strictly prohibited for profit.
```

## 运行截图

<img src='https://raw.githubusercontent.com/scarsty/kys-cpp/master/pic/1.png' />

<img src='https://raw.githubusercontent.com/scarsty/kys-cpp/master/pic/2.png' />

<img src='https://raw.githubusercontent.com/scarsty/kys-cpp/master/pic/3.png' />


## 其他

Created by SB500@www.dawuxia.net.

Special thanks to WangZi, NiBa, HuaKaiYeLuo, XiaoWu, LiuYunFeiYue, ZhenZhengDeQiangQiang, SanDaShan, SB250 and SB750.

A title "Powered by www.dawuxia.net" is advised to be displayed on the welcome screen.