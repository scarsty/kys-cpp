## 转换DOS版资源到kys的一般流程

### 图片资源

将“贴图”目录下的两个文件放到DOS游戏目录，执行批处理即可。

注意，其中的战斗文件去掉了闪烁效果。

你也可以查看批处理的内容，自己进行一些设定。

战斗贴图还需要动作帧数，下面abc工程中会有介绍。

不会特别处理物品图像，建议自己重新选图。

### 事件和对话转为lua脚本

查看“脚本”目录下的“data”目录，将其中的talk.grp、talk.idx、kdef.grp、kdef.idx替换为你要转换的dos版文件。

执行TalkMaker.exe，将对话导出为talkutf8.txt。

回到“脚本”目录，执行sfeKdef.exe，将事件导出为lua脚本。

也可以修改ini文件的内容进行一些自定义，但是建议不要作修改。

### abc工程的一些功能

因为转换基本是一次性的工作，对效率要求不高，但是需避免出错。故abc工程建议使用VS打开，在其中用Debug模式运行，

该工程最好用C++23或更新标准编译。

#### 转换存档为32位，生成数据库，以及战斗帧数

使用expandR函数，用法为：
```c++
//扩展存档，将短整数扩展为int32
// idx: 即idx文件
// grp：即grp文件
// index：转为数据库的存档编号
// ranger：若设置为否，则只转换战斗帧数（生成战斗帧数失败时，再次运行可设置为否）
// make_fightframe：若设置为是，则生成战斗帧数文本
int expandR(std::string idx, std::string grp, int index, bool ranger = true, bool make_fightframe = false)
```
如需生成数据库，在此函数前应调用initDBFieldInfo()

战斗帧数生成一次就够了。

#### 转换二进制列表

将离队列表，升级经验转为文本文件。

用法为：

```c++
    trans_bin_list(path + "list/levelup.bin", path + "list/levelup.txt");
    trans_bin_list(path + "list/leave.bin", path + "list/leave.txt");
```
这两个文件通常来自pascal复刻版，某些lua版后来也有沿用。

实际上这两个就够了。

kys并没有处理武功武器配合。

#### 验证战斗帧数

```c++
//验证战斗帧数的正确性
void check_fight_frame(std::string path, int repair = 0)
```
主要用于检查帧数与贴图数是否一致，不能保证结果完全正确。

需在生成帧数列表后执行。

#### 转换战斗帧数格式

```c++
//导出战斗帧数为文本
void trans_fight_frame(std::string path0)
```
仅用于转换金庸水浒传及相关MOD的战斗帧数格式。

#### 合并index.ka

```c++
void combine_ka(std::string in, std::string out)
```
用于将smp和wmp的index.ka合并。

#### 编号人物头像

```c++
//重新产生头像
void make_heads(std::string path)
```

从一个头像库里面选出头像，命名为初始存档的编号。

头像库可以从其他地方获取，git上没有保存。

更建议自行重配头像。

#### 检查脚本文件正确性

```c++
//检查3号指令的最后3个参数正确性
void check_script(std::string path)
```

只检查3号指令的最后两个参数是否合理。因修改器默认值问题，部分MOD对此参数处理有误，但DOS版的bug会使结果正确。复刻版均修正了此问题。


以上功能在abc工程的main函数中均有调用范例，注意可能因为多次执行会被注释掉。

