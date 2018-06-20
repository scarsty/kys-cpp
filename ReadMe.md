# kys-cpp

github：https://github.com/scarsty/kys-cpp

码云（不定期同步）：https://gitee.com/scarsty/kys-cpp

这是一个以SDL2为基础实现的2D游戏框架。

基本按照当代游戏引擎的思路实现，但是没有使用回调，因为回调会增加初学者的使用难度。

同时相当于提供了一个使用该框架制作DOS游戏《金庸群侠传》移植版的范例。

资源文件：<http://pan.baidu.com/s/1sl2X9wD>

Windows下可以使用Visual Studio编译，其他系统下可以使用CMake生成Makefile，使用Clang或者GCC编译。

## 架构的简单说明

### 公用类

Engine封装了一套SDL2的主要实现，主要取自TinyPot。

File是一些读取，写入函数。包含几个简化程序的模板函数。

Save中对所有数据进行了封装，可以较为方便地调用。

TextureManger是一个纹理管理器，因为《金庸群侠传》的贴图是含偏移设置的，故有些特殊的地方。

Audio是音频类，基于Bass，可以播放mid，mp3，wav等。

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

tinypot <https://github.com/scarsty/tinypot>

libass <https://github.com/libass/libass>

fribidi <https://www.fribidi.org/>

ini Reader <https://github.com/benhoyt/inih>

common <https://github.com/scarsty/common>

除BASS和BASSMIDI为闭源，但可以免费用于非商业项目之外，其他均为开源工程。

部分库和对应的头文件可以从<https://github.com/scarsty/local-lib>取得。

### Common

common目录包含了一些常用的公共功能，被多个工程使用。为了维护方便，这里是一个指向其他目录的软链接。实际的代码请从<https://github.com/scarsty/common>下载并置于适合的位置，或者参考get-common.sh中的命令。

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

<img src='https://pic2.zhimg.com/80/v2-fcac09adf861ee474477bbe91bf0fbab_hd.jpg' />

<img src='https://pic1.zhimg.com/80/v2-e62f9410ab6188ce8bb54cf9b7f745a3_hd.jpg' />

<img src='https://pic2.zhimg.com/80/v2-37bc164bdcaa175c65bce841d134f981_hd.jpg' />
