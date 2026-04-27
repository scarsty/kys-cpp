// transk.cpp — 将 kdef 二进制指令流翻译为 Lua 文本
//
// kdef 存储的是定长指令流，每条指令由 ins.length 个 int16 组成。
// ini 文件记录各指令的属性（名称/长度/跳转偏移/是否条件跳）。
// transk() 输入整条指令流，输出可人读 Lua 脚本字符串。

#include <iostream>
#include <string>
#include <vector>

#include "GrpIdxFile.h"
#include "filefunc.h"
#include "potconv.h"
#include "strfunc.h"

#include "INIReader.h"

std::vector<std::string> get_talks(std::string path, std::string coding)
{
    std::vector<int> offset, length;
    auto talk = GrpIdxFile::getIdxContent(path + "/talk.idx", path + "/talk.grp", &offset, &length);
    for (int i = 0; i < offset.back(); i++)
    {
        if (talk[i])
        {
            talk[i] = talk[i] ^ 0xff;
        }
    }
    std::vector<std::string> talk_contents;
    for (int i = 0; i < length.size(); i++)
    {
        std::string str = strfunc::replaceAllSubString(PotConv::conv(talk.data() + offset[i], coding, "utf-8"), "*", "");
        strfunc::replaceAllSubStringRef(str, "\r", "");
        strfunc::replaceAllSubStringRef(str, "\n", "");
        strfunc::replaceAllSubStringRef(str, "\"", (char*)u8"”");
        talk_contents.push_back(str);
    }
    return talk_contents;
}

std::vector<std::string> get_talk_utf8(std::string filename)
{
    std::string content = filefunc::readFileToString(filename);
    strfunc::replaceAllSubStringRef(content, "\r", "");
    std::vector<std::string> lines = strfunc::splitString(content, "\n", false);
    return lines;
}

// 一条 kdef 指令的属性描述
struct Instruct
{
    int id;
    std::string name;    // 指令名称模板（含 $0/$1 占位符，展开时替换为实际参数）
    int length;          // 指令占用的 int16 个数（包含指令码本身）
    int is_bool;         // 是否是条件指令（含条件跳转）
    int jump1;           // 条件为真时的跳转偏移在指令中的下标
    int jump2;           // 条件为假时的跳转偏移在指令中的下标
};

// 全局指令表（读自 kdef.ini [KdefAttrib]）和对话表（读自 talkutf8.txt）
std::map<int, Instruct> ins_;
std::vector<std::string> talks_;

// 将 talk.grp/idx 中的对话解密、编码转换并写入 talkutf8.txt
void trans_talks(std::string talk_path, std::string coding)
{
    auto talk_utf8 = get_talks(talk_path, coding);

    std::string str;
    for (const auto& talk : talk_utf8)
    {
        str += talk + "\n";
    }
    filefunc::writeStringToFile(str, talk_path + "/talkutf8.txt");
}

// 从 kdef.ini 加载指令表，从 talkutf8.txt 加载对话表。
// 两者均加载到全局变量 ins_ / talks_。
// 读取顺序必须先 trans_talks 生成 talkutf8.txt，再调用 init_ins。
void init_ins(std::string ini_file, std::string talkfile)
{
    INIReaderNormal ini;
    ini.loadFile(ini_file);
    int guess_count = ini.getAllKeys("KdefAttrib").size();
    for (int i = -1; i < guess_count + 5; i++)
    {
        if (!ini.hasKey("KdefAttrib", std::to_string(i)))
        {
            continue;
        }
        Instruct& ins = ins_[i];

        ins.id = i;
        auto str = ini.getString("KdefAttrib", std::to_string(i), "");

        auto strs = strfunc::splitString(str, "|", false);
        ins.name = strs[5] + ";";
        ins.length = std::stoi(strs[1]);
        ins.is_bool = std::stoi(strs[2]);
        ins.jump1 = std::stoi(strs[3]);
        ins.jump2 = std::stoi(strs[4]);
    }

    if (filefunc::fileExist(talkfile))
    {
        talks_ = get_talk_utf8(talkfile);
    }
    else
    {
        printf("Please make talkutf8.txt first!\n");
        exit(1);
    }
    if (ins_.empty())
    {
        printf("Error of ini file!\n");
        exit(1);
    }
}

// 将一个事件的指令流（int16 数组）翻译为 Lua 文本。
// lines 是以指令入口位置为 key 的有序 map；
// 条件跳转目标位置用 "当前位置 + 偏移 - 0.5" 作为 key 插入标签，
// 确保标签行排列在被跳转指令之前。
std::string transk(std::vector<int> e)
{
    int i = 0, i_line = 0;
    // key 为指令入口字节位置；条件跳转标签用位置 - 0.5 的 double 键插入，
    // 这样标签行在迭代时自然排列在目标指令入口行之前
    std::map<double, std::string> lines;    //索引为指令的位置，方便处理偏移
    while (i < e.size())
    {
        i_line++;
        int ins_id = e[i];
        auto& ins = ins_[ins_id];

        std::string str = ins.name;

        if (ins.id == 3)
        {
            //修正3号指令的错误
            if (e[i + 1] >= 0)
            {
                if (e[i + 12] == 0) { e[i + 12] = -2; }
                if (e[i + 13] == 0) { e[i + 13] = -2; }
            }
        }

        if (ins.is_bool)
        {
            int jump, jump1 = 0;
            if (e[i + ins.jump1] == 0)
            {
                strfunc::replaceAllSubStringRef(str, "bool", "false");
                jump = e[i + ins.jump2];
            }
            else if (e[i + ins.jump2] == 0)
            {
                strfunc::replaceAllSubStringRef(str, " == bool", "");
                jump = e[i + ins.jump1];
            }
            else
            {
                strfunc::replaceAllSubStringRef(str, " == bool", "");
                strfunc::replaceAllSubStringRef(str, "end", "else goto label$y end");
                jump = e[i + ins.jump1];
                jump1 = e[i + ins.jump2];
            }

            double index = i + ins.length + jump - 0.5;
            std::string label;
            if (lines.contains(index))
            {
                label = lines[index].substr(2, lines[index].length() - 4);
            }
            else
            {
                label = std::format("label{}", i);
            }
            strfunc::replaceAllSubStringRef(str, "label$x", label);
            lines[index] = "::" + label + "::";
            if (jump1 != 0)
            {
                double index1 = i + ins.length + jump1 - 0.5;
                std::string label1;
                if (lines.contains(index1))
                {
                    label1 = lines[index1].substr(2, lines[index1].length() - 4);
                }
                else
                {
                    label1 = std::format("label{}_1", i);
                }
                strfunc::replaceAllSubStringRef(str, "label$y", label1);
                lines[index1] = "::" + label1 + "::";
            }
        }

        while (1)
        {
            if (str.contains("talk($"))
            {
                auto pos = str.find("talk($");
                int num = std::stoi(str.substr(pos + 6, str.find(")", pos) - pos - 6));
                strfunc::replaceAllSubStringRef(str, std::format("talk(${})", num), talks_[e[i + num + 1]]);
            }
            else
            {
                break;
            }
        }

        for (int k = 1; k < ins.length; k++)
        {
            strfunc::replaceAllSubStringRef(str, std::format("${:x}", k - 1), std::to_string(e[i + k]));
        }

        lines[i] = str;
        i += ins.length;
    }
    std::string content;

    //多余的尾部处理掉
    auto it = lines.end();
    it--;
    if (it->second == "exit();")
    {
        lines.erase(it);
    }
    for (const auto& [id, line] : lines)
    {
        if (line.empty() || line == ";")
        {
        }
        else
        {
            content += line + "\n";
            //printf("%s;\n", line.c_str());
        }
    }
    if (!content.empty() && content.back() == '\n')
    {
        content.pop_back();    //去掉最后一个换行符
    }
    return content;
}