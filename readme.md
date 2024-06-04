# kys-cpp

金庸群侠传复刻版，为区别于其他语言的复刻版，添加后缀cpp。

<img src='https://raw.githubusercontent.com/scarsty/kys-cpp/master/pic/title.jpg' />

除了经典的回合制战斗外，还包含半即时战斗（含进度条），以及两种完全即时战斗模式。即模仿黑帝斯和只狼的战斗系统。可以通过修改ini文件中的battle_mode来切换。

github：https://github.com/scarsty/kys-cpp

码云（不定期同步）：https://gitee.com/scarsty/kys-cpp

资源文件：<http://pan.baidu.com/s/1sl2X9wD>

这是一个以SDL2为基础实现的2D游戏框架，同时相当于提供了一个使用该框架制作DOS游戏《金庸群侠传》移植版的范例。

## 如何编译

因使用了概念，需至少C++20。

Windows下建议先安装vcpkg，并在vcpkg目录中执行：
```bat
.\vcpkg install sdl2:x64-windows sdl2-image:x64-windows sdl2-ttf:x64-windows sdl2-mixer:x64-windows lua:x64-windows opencc:x64-windows sqlite3:x64-windows libiconv:x64-windows asio:x64-windows picosha2:x64-windows yaml-cpp:x64-windows libzip:x64-windows
```
建议执行：
```bat
.\vcpkg.exe integrate install
```
获取子模块mlcc：

```shell
git submodule init
git submodule update
# 可选
git submodule update --remote --rebase
```
之后使用Visual Studio（尽量用新版）打开kys.sln，编译即可。工程为x64版本，如需要x86版请自行修改。

上面的方法不含播放视频功能。如需要此功能，例如播放开场动画，则需先编译smallpot的动态库，比较复杂，请与作者联系。

Linux下编译参考doc目录中的文档。需注意没有联机对战部分。

对依赖的详细解释见doc目录中的dependencies.md。

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

