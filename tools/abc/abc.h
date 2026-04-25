#pragma once

// abc.h — DOS 版金庸群侠传资源转换工具的主要接口
// 负责将 16 位二进制存档/资源扩展为 32 位格式，并导出到 SQLite 数据库

#include <string>

// 初始化用于 SQLite 导出的字段描述表（各数据表的字段名、类型、偏移）
void initDBFieldInfo();

// 将二进制列表文件（如 levelup.bin / leave.bin）转换为逗号分隔的文本
void trans_bin_list(std::string in, std::string out);

// 遍历 fight000~fight900 目录，将 fightframe.ka 转换为可读的 fightframe.txt
void trans_fight_frame(std::string path0);

// 递归遍历路径，将所有 index.ka（sprite 偏移表）转换为 index.txt；
// zip 包内的 index.ka 也会被处理
void trans_all_index_ka(std::string path0);

// 将存档数据写入 SQLite 数据库（<num>.db），供外部工具查看/编辑
void saveR0ToDB(int num, std::string path);

// 核心转换：将 16 位 grp/idx 存档扩展为 32 位格式（grp32/idx32），
// 同时将 CP950 字符串转为 UTF-8，并导出 SQLite 数据库。
// ranger=false 时只生成战斗帧数文本，不做其余扩展。
int expandR(std::string idx, std::string grp, int index, std::string path = "./", bool ranger = true, bool make_fightframe = false);

// 将 in 目录中存在而 out 目录缺失的文件复制过去（用于合并 wmap→smap）
void combine_image_path(std::string in, std::string out);

// 将 in 的 index.ka 中非零条目合并到 out 的 index.ka（保留 out 已有非零值）
void combine_ka(std::string in, std::string out);

// 检查所有角色的战斗帧数总和是否与目录下 PNG 数量匹配；
// repair != 0 时自动修正只有一种帧的情况
void check_fight_frame(std::string path, int repair = 0);

// 检查 path 下所有脚本文件的基本格式（调试用）
void check_script(std::string path);

// 从旧版资源目录批量提取头像 PNG（调试用）
void make_heads(std::string path);

// 将 eft（特效）二进制文件按 eftlist_bin 中的条目拆分到独立目录
void split_eft_file(std::string path, std::string eftlist_bin);
