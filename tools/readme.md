# tools

xia、trans50、abc用于转换旧版资源。

cuthug是一个例程，用于将整张大地图地面切成8*8分块的例程，参看“高清素材的方案”。

## xia

贴图转换工具，用法为：

```
xia.exe idx文件名 grp文件名 输出目录 是否需要闪烁（0或1）
```

一般将exe和批处理文件放到DOS游戏目录，直接执行即可。

注意，战斗文件和头像文件是不需要闪烁的。

你也可以查看批处理的内容，自己进行一些设定。

战斗贴图还需要动作帧数，下面abc工程中会有介绍。

不会特别处理物品图像，建议自己重新选图。

## trans50

包含3个功能，覆盖了原sfe2kdefscript，talkmaker的功能。

需注意该工具的一些激进方案转换出的脚本有可能会不能正确执行。

### 将原版对话转为txt文件

```
trans50 --talk --in path
```
其中path为talk.idx和talk.grp所在的文件夹，编码限制为BIG5。执行之后会在path文件夹中生成talkutf8.txt文件，每个对话一行。

需注意如果用编辑器查看的话，行号为对话的编号加一。

该功能会去掉*，改为游戏中自动计算换行。

默认的编码为cp950，可以通过--talkcoding传入其他编码。

如果有其他需求请自己修改代码处理。


### 将原版指令转为lua脚本

```
trans50 --kdef --in path --talkfile talkfile --out path_out
```
其中path为kdef.idx、kdef.grp所在的文件夹，talkfile为talkutf8.txt的路径，应预先生成。

执行之后会在输出目录中的event文件夹得到所有剧情事件的lua脚本。这些脚本可以被kys-pascal和kys-cpp直接支持。需注意kys-cpp的50指令部分支持不完整。

也可以自己修改ini文件，得到适合自己版本的脚本。

### 将50指令转成可读性稍好的程序段

需注意这一步转换后的结果不保证能正确执行。

```
trans50 --50 --in path --talkfile talkfile --out path_out
```
path为上一步转换脚本的文件夹，talkfile为talkutf8.txt的路径。

执行之后会在输出目录的event50文件夹下面得到50指令翻译后的脚本，如果原脚本不含50指令则不会有输出。

此处得到的脚本更容易阅读，且大部分能被kys-pascal执行，但因一些关键字不同，不保证能完全正常执行。如有进一步需求建议自行修改相关代码。

转换过程会自动将向前的 goto 跳转改写为 `if ... then ... end` 结构。向后的 goto（循环）因转换效果不稳定，建议用AI来帮忙转换成结构化的代码。

但是需注意50指令变量所在的位置与原来不同，故不能混用转换前后的脚本。

### 批处理范例

```bat
trans50 --talk --in %1
trans50 --kdef --in %1 --talkfile %1/talkutf8.txt
trans50 --50 --in ./event --talkfile %1/talkutf8.txt
```

### 原地转换

若增加overwrite参数，则会直接覆盖原文。请慎用。

### 事件和对话转为lua脚本

使用tools中的trans50命令行工具进行转换：

可以直接调用此处的批处理，即：

```
maketalk.bat path
make.bat path 
```
其中path即kdef和talk（包含idx和grp）所在的目录。

50指令及转译目前不能保证正确执行。

## abc

用于转换旧的存档和列表等

因为转换基本是一次性的工作，对效率要求不高，但是需避免出错。故有些功能建议使用VS打开，在其中用Debug模式运行。

该工程需要用C++23或更新标准编译。

### 转换存档为32位，生成数据库，以及战斗帧数

需注意先转换好fight的图片，才能生成战斗帧数。

一般使用以下命令就可以生成。如果保存的路径不一样，也可以用--path参数传递进去。更详细的内容请查看源码。

```
.\abc.exe --save
```

### 递归转换index.ka为index.txt

因为index.ka是二进制文件，且每两个int16为一组，比较麻烦，因此提供了这个功能来转换成文本格式，方便查看和修改。

```
.\abc.exe --trans-indexka --path 资源目录
```

会递归查找目录下所有`index.ka`，以及zip中的`index.ka`，并生成同位置的`index.txt`。

`index.txt`每行格式为：

```
编号: 数字1, 数字2
```

其中编号从0开始，按`index.ka`每两个`int16`为一组的顺序递增。不存在的编号视为两个0。

编号可以不按顺序。

### 验证战斗帧数

```c++
.\abc.exe --check-fightframe
```
主要用于检查帧数与贴图数是否一致，正确情况下总帧数应是四个战斗帧数加起再乘以4，但计算结果一致并不保证结果完全正确。

错误的部分建议手动修正。

需在生成帧数文件后执行。

### 合并wmp到smp

```shell
.\abc.exe --combine-wmpsmp
```
用于将smp和wmp的图片资源和index.ka合并。

### 分拆武功光影效果图

eft图片在导出时为一个目录，分拆后可以更方便地替换和添加。但是这一步需要pascal复刻版的effect.bin文件。如果没有，请从z.dat文件中将这个列表手动复制出来。

```shell
.\abc.exe --split-eft
```

更建议重配eft图片。

## 后面的内容不容易自动完成，最好是在VS下面自己修改代码，用Debug来运行

### 转换二进制列表

将离队列表和升级经验转为文本文件。

用法为：

```c++
trans_bin_list(path + "list/levelup.bin", path + "list/levelup.txt");
trans_bin_list(path + "list/leave.bin", path + "list/leave.txt");
```
这两个文件通常来自pascal复刻版，某些lua版后来也有沿用。

实际上这两个就够了。

kys并没有处理武功武器配合。

### 编号人物头像

```c++
//重新产生头像
void make_heads(std::string path)
```

从一个头像库里面选出头像，命名为初始存档的编号。

头像库可以从其他地方获取，git上没有保存。

更建议自行重配头像。

### 检查脚本文件正确性

```c++
//检查3号指令的最后2个参数正确性
void check_script(std::string path)
```
只检查3号指令的最后两个参数是否合理。因修改器默认值问题，部分MOD对此参数处理有误，但DOS版的bug会使结果正确。复刻版修正了这个问题，故可能导致旧的部分剧情出错误。前述的转为lua脚本工具会处理，此处为验证。

### 转换战斗帧数格式

```c++
//导出战斗帧数为文本
void trans_fight_frame(std::string path0)
```
仅用于转换金庸水浒传及相关MOD的战斗帧数格式。一般不需要。