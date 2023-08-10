## 如何在 linux 上编译
本作支持 linux 32/64 bit 环境，以本机示范如何编译。

### 编译环境
本机 OS 是 x86 ubuntu18.04，菜鸡配置，不需要显卡。注意本作需要 c++17。
```shell
$ uname -a
Linux BJ-DZ0103437 5.4.0-47-generic #51~18.04.1-Ubuntu SMP Sat Sep 5 14:35:50 UTC 2020 x86_64 x86_64 x86_64 GNU/Linux

$ g++ --version
g++ (Ubuntu 7.5.0-3ubuntu1~18.04) 7.5.0
Copyright (C) 2017 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```
### 基础依赖

子模块
```shell
git submodule init
git submodule update
# 可选
git submodule update --remote --rebase
```

这几个是 bass（播放语音用的） 运行依赖
```shell
sudo apt-get install libgtkd-3-dev libglade2-dev
```

以下是游戏本身依赖
```shell
sudo apt-get install libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev liblua5.3-dev libsdl2-dev libopencc-dev libzip-dev libsqlite3-dev libyaml-cpp-dev
```

查看 sdl 版本
```
$ sdl-config --version
2.0.8
```

### 下载配置 bass 和 bassmidi

> 非必须，也可以使用sdl2-mixer代替。

www.un4seen.com 选 linux 版本下载、解压。
* [点击下载 bass24](http://www.un4seen.com/download.php?bass24-linux)
* [点击下载 bassmidi24](http://www.un4seen.com/files/bassmidi24-linux.zip)

配置 bass so 和 .h 环境变量和 fmt1.h 路径
```shell
export BASS_HOME=${自己的bass解压路径}
export BASS_MIDI_HOME=${自己的bassmidi解压路径}
export CPATH=${BASS_HOME}:${BASS_MIDI_HOME}:${CPATH}
export LD_LIBRARY_PATH=${BASS_HOME}:${BASS_MIDI_HOME}:${LD_LIBRARY_PATH}
export export CPLUS_INCLUDE_PATH=${自己的kys-cpp路径}/nb:${CPLUS_INCLUDE_PATH}
```
注意 64bit 机器要链接的是 x64 目录下面的 so，这个压缩包下面先放的是 32bit 用的 so。`cp -rf x64/libbass.so libbass.so`覆盖掉即可。
```shell
$ cd /home/tpoisonooo/GitProjects/bass/bass24-linux/x64
$ file libbass.so 
libbass.so: ELF 64-bit LSB shared object, x86-64, version 1 (SYSV), dynamically linked, stripped

$ file ../libbass.so 
../libbass.so: ELF 32-bit LSB shared object, Intel 80386, version 1 (SYSV), dynamically linked, stripped
```

### build 和小改动
```bash
$ mkdir build
$ cmake ../src && make
```

* 如果提示找不到 SDL 的几个变量，例如 SDL_CONTROLLER xxx 。作者想支持手柄用了 sdl2.0.20 版本，ubuntu 目前没有现成的源。删掉对应代码即可。

* 如果 cmake 版本过高
```shell
$ cmake --version
cmake version 3.10.2

CMake suite maintained and supported by Kitware (kitware.com/cmake).
```
需要修改 src/CMakeLists.txt，增加一行`cmake_policy(SET CMP0015 OLD)`

* 大概率因为 fmt 版本问题，需要打开 `src/Audio.cpp`, 注释掉这 2 行：
若并没出现问题请跳过。
```c++
10 fmt1::print("Init Bass fault!\n");
```
```c++
16 fmt1::print("Mix_OpenAudio: {}\n", Mix_GetError());
```

* 然后 cmake 编译 
```shell
cd src
mkdir build && cd build
cmake .. && make -j 4
```

编译出的 bin 叫做`kys`

### 资源链接和运行

`kys` 需要搭配资源（贴图、脚本剧情、音乐等）才能正常运行。
资源从这里下载：
https://pan.baidu.com/s/1sl2X9wD

源码中文件名均为小写，故有需要手动将资源文件名全部转为小写。例如执行以下脚本（未测试）：

```shell
for f in *
do
nf=`echo $f | tr 'A-Z' 'a-z'`
echo $nf
[ "$f" != "$nf" ] && mv ./"$f" ./"$nf"
done
```

只需要下载里面的 `kys-c++.zip`，另一个 `clzr-c++.zip` 貌似是个半成品。
下载完解压，得到 game 目录。软链到 `kys` 的上一级（和`build`同级）
```shell
cd src
ln -s /home/tpoisonooo/baidunetdiskdownload/kys-c++/game
```
然后就可以运行了
```shell
cd src/build
./kys
```
### 一些小 BUG
1. 没法直接`开始游戏`，加载存档，貌似读第 3/4 也是游戏的开始；
2. 林厨子第二次对话`谁家玉笛听落梅`的时候，会自动进入和胡斐的战斗，背景是凌霄城；
3. 传送时小概率 core dump，好在本作有 autosave。至于怎么传送自己探索 +_+
4. 有些头像比较魔性，例如神雕大侠...
5. 战斗时偶尔有的人刚进入场景没有被渲染出来（运行时异常？），移动一次就好；
6. 几个山洞出小地图进大地图的时候，偶尔一次出不去，多跑跑，`没病走两步`。
