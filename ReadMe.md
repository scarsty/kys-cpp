# 金庸老先生千古！

# kys-cpp

github：https://github.com/scarsty/kys-cpp

码云（不定期同步）：https://gitee.com/scarsty/kys-cpp

资源文件：<http://pan.baidu.com/s/1sl2X9wD>

这是一个以SDL2为基础实现的2D游戏框架，同时相当于提供了一个使用该框架制作DOS游戏《金庸群侠传》移植版的范例。

Windows下可以使用Visual Studio编译，其他系统下可以在src目录使用CMake生成Makefile，使用Clang或者GCC编译。

## 架构的简单说明

### 公用类

Engine封装了一套SDL2的主要实现，主要取自TinyPot。

File是一些读取，写入函数。包含几个简化程序的模板函数。

Save中对所有数据进行了封装，可以较为方便地调用。

TextureManger是一个纹理管理器，因为《金庸群侠传》的贴图是含偏移设置的，故有些特殊的地方。

Audio是音频类，基于Bass，可以播放mid、mp3、wav等格式。

### Element

Element是游戏中的基本执行类，包含5个重要的虚函数：backRun，draw，dealEvent，onEnter，onExit。对应在背景中执行，如何画自身，如何处理事件，进入时的处理，退出时的处理。一般来说，衍生类应重写这些函数。

其中每个节点可以包含数个子节点，在绘图时子节点也会被自动一一绘出。需注意在画自身的部分不需要处理子节点，除非有特殊的需要。

存在一个全局的的Element栈root（实际是std::vector），会从下到上依次画出每个Element。Element类有一个占满全屏的属性，表示这个类将占用全部的屏幕，因此引擎在绘制的时候，会仅找出最靠上的含有该属性的节点，并从这里开始往上画。

创建一个节点，并调用run过程即可运行此节点，注意使用run执行的节点是完全独占的，其子节点也会有事件响应。如果需要退出当前节点，在适当的地方使用setExit(true)即可，但是子节点调用是无效的，除非拥有当前运行节点的指针。

run过程的参数为一个布尔值，如果为true则会被加入到root并进行绘制，如果为false则只运行不参与绘制。但是很多节点的draw过程是空的，即使放到root中也不会参与绘制，实际利用了这一特性的仅有显示人物对话的部分。

run过程会返回一个函数值，可以利用进行一些判断，例如菜单的选择。

自己创建的节点通常需要自己销毁，但是要注意如果某个节点在其他节点的Child当中，则会被自动销毁，请注意这些问题。

部分节点使用了单例，这些节点请留给程序运行结束自动销毁。

请不要让子节点出现递归包含，这样会迅速消耗掉所有资源。

### 注意事项

本工程刻意避免了一些C++新特性的使用，例如智能指针、内存管理等，而采用自己管理内存的方式。

通常来说，大部分游戏引擎都需要全局标记和回调来控制剧情的执行，本框架采用Element的run设计，使事件以阻塞的模式顺序执行，同时绘图仍是无阻塞执行的。

更多分析见<https://www.dawuxia.net/thread-1039649-1-1.html>。

## abc工程以及资源的保存

abc工程用来转换之前的数据。

其中主要的功能是将存档的R部分扩展为原来的二倍。即所有的16位整数转为32位整数，表示范围从32767扩大到2^31-1，足够通常的数值使用。同时，原有的字串也扩展为之前的二倍长度，例如原来人物的名字有5个中文字符长度，实际上最多只能使用4个字，转换之后则可以使用9个字（并不是推荐你用9个字）。转换之后的文件名变为r?.grp32。

文件的文本编码，仅有初始存档为cp950（BIG5），这是向下兼容的需要，但是内部会使用cp936（GBK），存档被保存后也会转为cp936。

游戏的资源文件是以单个图片的形式放在resource的各个目录中的，每张图的偏移保存在index.ka中，格式为每张图两个16位整数，连续存放。目前没有设计打包格式。

战斗贴图文件中，每个人的帧数，之前在hugebase（水浒）框架中使用fightframe.ka保存，现改用fightframe.txt保存。格式为动作索引（0~4），每方向数量。未写则视为0。

之前游戏使用的列表文件只保留了升级经验列表和离队列表，改用txt格式。

并非所有的文档都转为32位，这有一部分是为了节省资源的需要。

以上提到的数据，除了文本文件外均可以用真正的强强的新版upedit修改（该修改器不完善）。  

## 使用到的其他开发库

SDL <https://www.libsdl.org/>

SDL_image <https://www.libsdl.org/projects/SDL_image/>

libpng <http://www.libpng.org/pub/png/libpng.html>

SDL_ttf <https://www.libsdl.org/projects/SDL_ttf/>

freetype <https://www.freetype.org/>

BASS, BASSMIDI <http://www.un4seen.com/>

FFmpeg <https://www.ffmpeg.org/>

libiconv <https://www.gnu.org/software/libiconv/>

lua <https://www.lua.org/>

minizip <https://github.com/madler/zlib/tree/master/contrib/minizip>

zlib <https://zlib.net/>

libass <https://github.com/libass/libass>

fribidi <https://www.fribidi.org/>

ini Reader <https://github.com/benhoyt/inih>

OpenCC <https://github.com/BYVoid/OpenCC>

Fast C++ CSV Parser: <https://github.com/ben-strasser/fast-cpp-csv-parser>

asio (from boost)

PicoSHA2: <https://github.com/okdshin/PicoSHA2>

hanz2piny <https://github.com/yangyangwithgnu/hanz2piny>

除BASS和BASSMIDI为闭源，但可以免费用于非商业项目之外，其他均为开源工程。

汉字转拼音库直接将源码集成进了工程。

### common和local

common <https://github.com/scarsty/common>

common包含了一些常用的公共功能，是作者所写的一个通用功能集合，被多个工程使用。

前面提到的开源库大部分可以从<https://github.com/scarsty/lib-collection>取得头文件和导入库。此工程收集了一些Linux下常见，但是Windows下经常不能直接使用的库，在Windows下编译时通常需要将其保存在local目录中，有些库也可以用vcpkg或者msys2来安装，请自行选择处理。在Linux下编译时则应优先考虑使用系统自带的库。

PicoSHA2和CSV库仅需要头文件，请注意将它们复制到适合的位置。

可以用以下命令

```shell
git clone https://github.com/scarsty/common common
git clone https://github.com/scarsty/lib-collection local


mkdir include
cp ./local/include/picosha2.h ./include
cp ./local/include/csv.h ./include
```

或者get-submodule.sh。

### tinypot

tinypot <https://github.com/scarsty/tinypot>

这是作者编写的一个视频播放器，游戏中将其编译为动态库并用于进行视频的播放。

如果难以处理，可以将预处理定义宏中的\_TINYPOT删除。

## 授权

以zlib授权发布，但是包含两个附加条款：

一般情况下，可以自由使用代码。

但若将其用于基于《金庸群侠传》题材的游戏，则严禁任何形式的牟利行为。

Created by SB500@www.dawuxia.net.

Special thanks to WangZi, NiBa, HuaKaiYeLuo, XiaoWu, LiuYunFeiYue, ZhenZhengDeQiangQiang, SB250 and ICE.

The source codes are distributed under zlib license, with two additional clauses.

Full right of the codes is granted if they are used in non-KYS related games.

If the codes are used in KYS related games, the game is strictly prohibited for profit.

A title "Powered by www.dawuxia.net" is advised to be displayed on the welcome screen.

## 运行截图

第一张图中武器的属性显示有错误，代码中已修正。

<img src='https://pic2.zhimg.com/80/v2-fcac09adf861ee474477bbe91bf0fbab_hd.jpg' />

<img src='https://pic1.zhimg.com/80/v2-e62f9410ab6188ce8bb54cf9b7f745a3_hd.jpg' />

<img src='https://pic2.zhimg.com/80/v2-37bc164bdcaa175c65bce841d134f981_hd.jpg' />