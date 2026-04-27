// trans50.cpp — 将 Lua 脚本中的 instruct_50 调用展开为可读的 Lua 语句
//
// DOS 版完全依赖内置指令系统，自定义指令均以 instruct_50(code, e1..e6) 形式出现。
// trans50() 识别这些行并将其替换为对应的 Lua 表达式，保留原注释。

#include <iostream>

#include "filefunc.h"
#include "strfunc.h"
#include <format>
#include <map>
#include <print>
#include <string>
#include <vector>

extern std::vector<std::string> talks_;

std::string trans50(std::string str)
{
    std::string result;

    strfunc::replaceAllSubStringRef(str, "\r", "");
    std::vector<std::string> lines = strfunc::splitString(str, "\n", false);

    int next_parameter = -1;
    // e_GetValue: 根据类型位 t 的第 bit 位判断 x 是直接常数还是数组索引
    //   位为 0 → 返回字符串形式的数字（常量）
    //   位为 1 → 返回 "x[x]" 形式（变量引用）
    auto e_GetValue = [&next_parameter](int bit, int t, int x) -> std::string
    {
        int i = t & (1 << bit);
        if (i == 0)
        {
            return std::to_string(x);
        }
        else
        {
            return std::format("x[{}]", x);
        }
    };
    auto is_number = [](int bit, int t, int x)
    {
        return (t & (1 << bit)) == 0;
    };
    // talks_ 已由 transk.cpp 的 init_ins() 加载，这里取引用避免拷贝
    //读对话
    std::vector<std::string>& talk_contents = talks_;

    for (auto& line : lines)
    {
        std::string str = line;
        if (line.find("instruct_50") != std::string::npos)
        {
            auto pos = line.find("instruct_50");
            auto nums = strfunc::findNumbers<int>(line.substr(pos + 12));
            if (nums.size() >= 7)
            {
                if (next_parameter >= 1 && next_parameter <= nums.size())
                {
                    nums[next_parameter - 1] = 9999999;
                    next_parameter = -1;
                }
                int code = nums[0];
                int e1 = nums[1];
                int e2 = nums[2];
                int e3 = nums[3];
                int e4 = nums[4];
                int e5 = nums[5];
                int e6 = nums[6];

                switch (code)
                {
                case 0:    // 常量赋値： x[e1] = e2
                    str = std::format("x[{}] = {};", e1, e2);
                    break;
                case 1:    // 数组写： x[e3 + e4] = e5
                    str = std::format("x[{} + {}] = {};", e3, e_GetValue(0, e1, e4), e_GetValue(1, e1, e5));
                    break;
                case 2:    // 数组读： x[e5] = x[e3 + e4]
                    str = std::format("x[{}] = x[{}+ {}];", e5, e3, e_GetValue(0, e1, e4));
                    break;
                case 3:    // 算数运算： x[e3] = x[e4] op e5（e2=0~4：+-*/%，e2=5：无符号除）
                {
                    std::string op = "+-*/%";
                    if (e2 >= 0 && e2 <= 4)
                    {
                        str = std::format("x[{}] = x[{}] {} {};", e3, e4, op[e2], e_GetValue(0, e1, e5));
                    }
                    else if (e2 == 5)
                    {
                        str = std::format("x[{}] = uint(x[{}]) / {};", e3, e4, e_GetValue(0, e1, e5));
                    }
                    break;
                }
                case 4:    // 比较运算，结果存入 jump_flag（e2=0~5：< <= == ~= >= >；e2=6/7：常量 true/false）
                {
                    std::vector<std::string> op = { "<", "<=", "==", "~=", ">=", ">" };
                    if (e2 >= 0 && e2 <= 5)
                    {
                        str = std::format("if x[{}] {} {} then jump_flag = true; else jump_flag = false; end;", e3, op[e2], e_GetValue(0, e1, e4));
                    }
                    else if (e2 == 6)
                    {
                        str = "jump_flag = true;";
                    }
                    else if (e2 == 7)
                    {
                        str = "jump_flag = false;";
                    }
                    break;
                }
                case 5:    // 清空 x[0..30000]
                {
                    str = "for i=0, 30000 do x[i]=0; end;";
                    break;
                }
                case 6: break;    // 无操作
                case 7: break;    // 无操作
                case 8:           // 加载对话内容到字符串变量
                {
                    if (is_number(0, e1, e2))
                    {
                        str = std::format("x[{}] = \"{}\";", e3, talk_contents[e2]);
                    }
                    else
                    {
                        str = std::format("x[{}] = GetTalk({});", e3, e_GetValue(0, e1, e2));
                    }
                    break;
                }
                case 9:    // 字符串格式化（string.format）
                {
                    str = std::format("x[{}] = string.format(x[{}], {});", e2, e3, e_GetValue(0, e1, e4));
                    break;
                }
                case 10:    // 计算字符串实际显示宽度
                {
                    str = std::format("x[{}] = DrawLength(x[{}]);", e2, e1);
                    break;
                }
                case 11:    // 字符串拼接
                {
                    str = std::format("x[{}] = x[{}]..x[{}];", e3, e1, e2);
                    break;
                }
                case 12:    // 生成指定宽度的空格字符串
                {
                    str = std::format("x[{}] = string.rep(\" \", {});", e2, e_GetValue(0, e1, e3));
                    break;
                }
                case 16:    // 设置游戏对象属性（角色/物品/子场景/武功/商店）
                {
                    std::vector<std::string> names = { "Role", "Item", "SubmapInfo", "Magic", "Shop" };
                    str = std::format("Set{}({}, {} / 2, {});", names[e2], e_GetValue(0, e1, e3), e_GetValue(1, e1, e4), e_GetValue(2, e1, e5));
                    break;
                }
                case 17:    // 读取游戏对象属性
                {
                    std::vector<std::string> names = { "Role", "Item", "SubmapInfo", "Magic", "Shop" };
                    str = std::format("x[{}] = Get{}({}, {} / 2);", e5, names[e2], e_GetValue(0, e1, e3), e_GetValue(1, e1, e4));
                    break;
                }
                case 18:    // 设置队伍成员
                {
                    str = std::format("SetTeam({}, {});", e_GetValue(0, e1, e2), e_GetValue(0, e1, e3));
                    break;
                }
                case 19:    // 读取队伍成员
                {
                    str = std::format("x[{}] = GetTeam({});", e3, e_GetValue(0, e1, e2));
                    break;
                }
                case 20:    // 获取背包中物品数量
                {
                    str = std::format("x[{}] = GetItemAmount({});", e3, e_GetValue(0, e1, e2));
                    break;
                }
                case 21:    // SetD: 写入主地图对象属性
                {
                    str = std::format("SetD({}, {}, {}, {});", e_GetValue(0, e1, e2), e_GetValue(1, e1, e3), e_GetValue(2, e1, e4), e_GetValue(3, e1, e5));
                    break;
                }
                case 22:    // GetD: 读取主地图对象属性
                {
                    str = std::format("x[{}] = GetD({}, {}, {});", e5, e_GetValue(0, e1, e2), e_GetValue(1, e1, e3), e_GetValue(2, e1, e4));
                    break;
                }
                case 23:    // SetS: 写入子场景对象属性
                {
                    str = std::format("SetS({}, {}, {}, {}, {});", e_GetValue(0, e1, e2), e_GetValue(1, e1, e3), e_GetValue(2, e1, e4), e_GetValue(3, e1, e5), e_GetValue(4, e1, e6));
                    break;
                }
                case 24:    // GetS: 读取子场景对象属性
                {
                    str = std::format("x[{}] = GetS({}, {}, {}, {});", e6, e_GetValue(0, e1, e2), e_GetValue(1, e1, e3), e_GetValue(2, e1, e4), e_GetValue(3, e1, e5));
                    break;
                }
                case 25:    // write_mem: 写入 DOS 内存地址（历史兼容指令）
                {
                    if (e3 < 0)
                    {
                        e3 += 65536;    //处理负数地址
                    }
                    str = std::format("write_mem(0x{:x} + {}, {});", e3 + e4 * 0x10000, e_GetValue(1, e1, e6),
                        e_GetValue(0, e1, e5));
                    break;
                }
                case 26:    // read_mem: 读取 DOS 内存地址
                {
                    if (e3 < 0)
                    {
                        e3 += 65536;    //处理负数地址
                    }
                    str = std::format("x[{}] = read_mem(0x{:x} + {});", e5, e3 + e4 * 0x10000, e_GetValue(0, e1, e6));
                    break;
                }
                case 27:    // 获取对象名称字符串
                {
                    std::vector<std::string> names = { "Role", "Item", "Submap", "Magic", "Shop" };
                    str = std::format("x[{}] = Get{}Name({});", e4, names[e2], e_GetValue(0, e1, e3));
                    break;
                }
                case 32:    // 读取参数到 temp，同时记录下一条指令需替换的参数位置
                {
                    str = std::format("temp = x[{}];", e2);
                    e_GetValue(0, e1, e3);
                    next_parameter = e3;
                    break;
                }
                case 33:    // DrawString: 展示文本（同时清除 next_parameter 占位符）
                {
                    str = std::format("DrawString(x[{}], {}, {}, {});", e2, e_GetValue(0, e1, e3), e_GetValue(1, e1, e4), e_GetValue(2, e1, e5));
                    next_parameter = -1;
                    break;
                }
                case 34:    // DrawRect: 画矩形
                {
                    str = std::format("DrawRect({}, {}, {}, {});", e_GetValue(0, e1, e2), e_GetValue(1, e1, e3), e_GetValue(2, e1, e4), e_GetValue(3, e1, e5));
                    break;
                }
                case 35:    // 等待键盘输入
                {
                    str = std::format("x[{}] = GetKey();", e1);
                    break;
                }
                case 36:    // 展示消息对话框，返回用户选择
                {
                    str = std::format("x[28672] = showmessage(x[{}], {}, {}, {});", e2, e_GetValue(0, e1, e3), e_GetValue(1, e1, e4), e_GetValue(2, e1, e5));
                    break;
                }
                case 37:    // 延迟指定帧数
                {
                    str = std::format("Delay({});", e_GetValue(0, e1, e2));
                    break;
                }
                case 38:    // 生成随机数
                {
                    str = std::format("x[{}] = math.random({});", e3, e_GetValue(0, e1, e2));
                    break;
                }
                case 39:    // 展示选单（e2 个选项）
                case 40:    // 展示选单（同 case 39，参数约定略有不同）
                {
                    str = "strs = {};\n";
                    str += std::format("for i=1, {} do\n", e_GetValue(0, e1, e2));
                    str += std::format("strs[i] = x[x[{} + i - 1]];\n", e3);
                    str += "end\n";
                    str += std::format("x[{}] = menu({}, {}, strs, #strs);", e4, e_GetValue(1, e1, e5), e_GetValue(2, e1, e6));
                    break;
                }
                case 41:
                {
                    //画图（e2=0：主图，e2=1：头像）
                    if (e2 == 0)
                    {
                        str = std::format("DrawMainImage({} / 2, {}, {});", e_GetValue(2, e1, e5), e_GetValue(0, e1, e3), e_GetValue(1, e1, e4));
                    }
                    else if (e2 == 1)
                    {
                        str = std::format("DrawHeadImage({} / 2, {}, {});", e_GetValue(2, e1, e5), e_GetValue(0, e1, e3), e_GetValue(1, e1, e4));
                    }
                    break;
                }
                case 43:    // 设置事件参数并调用子事件
                {
                    str = std::format("x[28928] = {};\n", e_GetValue(1, e1, e3));
                    str += std::format("x[28929] = {};\n", e_GetValue(2, e1, e4));
                    str += std::format("x[28930] = {};\n", e_GetValue(3, e1, e5));
                    str += std::format("x[28931] = {};\n", e_GetValue(4, e1, e6));
                    str += std::format("CallEvent({});", e_GetValue(0, e1, e2));
                    break;
                }
                }
            }
            line = std::format("{}{}", std::string(0, ' '), str);
        }
        else
        {
            if (next_parameter > 0)
            {
                auto pos_l = line.find("(");
                auto pos_r = line.find(")");
                if (pos_l != std::string::npos && pos_r != std::string::npos && pos_l < pos_r)
                {
                    std::string params = line.substr(pos_l + 1, pos_r - pos_l - 1);
                    std::vector<std::string> param_list = strfunc::splitString(params, ",", true);
                    if (next_parameter > 0 && next_parameter <= param_list.size())
                    {
                        param_list[next_parameter - 1] = "9999999";
                        next_parameter = -1;
                    }
                    auto not_space = line.find_first_not_of(" \t");
                    str = line.substr(not_space, pos_l - not_space + 1);
                    for (int i = 0; i < param_list.size(); i++)
                    {
                        std::string param = param_list[i];
                        str += param;
                        if (i != param_list.size() - 1)
                        {
                            str += ", ";
                        }
                    }
                    str += line.substr(pos_r);
                }
                else
                {
                    str = line;
                }
            }
            line = str;
        }
        strfunc::replaceOneSubStringRef(line, "9999999", "temp");
        strfunc::replaceOneSubStringRef(line, "CheckRoleSexual(256)", "jump_flag");
        //printf("%s\n", str.c_str());
    }

    // 辅助：去除首尾空白（原地修改并返回引用）
    auto trim = [](std::string& s) -> std::string&
    {
        auto pos = s.find_first_not_of(" \t");
        if (pos != std::string::npos)
        {
            s.erase(0, pos);
        }
        pos = s.find_last_not_of(" \t");
        if (pos != std::string::npos)
        {
            s.erase(pos + 1);
        }
        return s;
    };

    // 辅助：对 Lua 条件取反（原地修改并返回 trim 后的值）
    auto make_reverse = [&](std::string& s) -> std::string&
    {
        std::vector<std::pair<std::string, std::string>> opr = {
            { "== false", "" }, { "== true", "== false" },
            { "<=", ">" }, { "==", "~=" }, { "~=", "==" },
            { ">=", "<" }, { "<", ">=" }, { ">", "<=" }
        };
        bool dealed = false;
        for (auto& [k, v] : opr)
        {
            if (s.contains(k))
            {
                strfunc::replaceAllSubStringRef(s, k, v);
                dealed = true;
                break;
            }
        }
        if (!dealed)
        {
            s = trim(s) + " == false";
        }
        return trim(s);
    };

    // 合并 jump_flag 条件到上一行的 if，减少中间变量
    for (int i = 1; i < (int)lines.size(); i++)
    {
        auto& line = lines[i];
        if (line.contains("jump_flag") && line.contains("if") && line.contains("then") && line.contains("goto") && !line.contains("else"))
        {
            auto& line0 = lines[i - 1];
            if (line0.contains("if") && line0.contains("then"))
            {
                auto posif = line0.find("if");
                auto posthen = line0.find("then");
                auto condition = line0.substr(posif + 2, posthen - posif - 2);
                trim(condition);
                if (line.contains("jump_flag == false"))
                {
                    strfunc::replaceAllSubStringRef(line, "jump_flag == false", make_reverse(condition));
                }
                else
                {
                    strfunc::replaceAllSubStringRef(line, "jump_flag", condition);
                }
                line0.clear();
            }
        }
    }

    // 将向前跳转（if ... goto labelN end）转换为 if ... then ... end 结构
    for (int i = 0; i < (int)lines.size(); i++)
    {
        auto& line = lines[i];
        if (line.contains("if") && line.contains("goto") && !line.contains("else"))
        {
            auto pos_goto = line.find("goto");
            auto pos_end = line.find("end", pos_goto);
            auto label = line.substr(pos_goto + 4, pos_end - pos_goto - 4);
            trim(label);

            auto pos_if = line.find("if");
            auto pos_then = line.find("then", pos_if);
            auto condition = line.substr(pos_if + 2, pos_then - pos_if - 2);
            trim(condition);

            // 只处理向前跳转（label 出现在当前行之后）
            for (int j = i + 1; j < (int)lines.size(); j++)
            {
                if (lines[j].find("::" + label + "::") != std::string::npos)
                {
                    lines[j] += "\nend;";
                    line = "if " + make_reverse(condition) + " then";
                    break;
                }
            }
        }
    }

    // 合并 temp 中间变量到使用它的下一行
    for (int i = 1; i < (int)lines.size(); i++)
    {
        auto& line = lines[i];
        if (line.contains("temp"))
        {
            auto& line0 = lines[i - 1];
            if (line0.contains("temp ="))
            {
                auto pos_eq = line0.find("=");
                auto pos_end = line0.find(";", pos_eq);
                auto value = line0.substr(pos_eq + 1, pos_end - pos_eq - 1);
                trim(value);
                strfunc::replaceAllSubStringRef(line, "temp", value);
                line0.clear();
            }
        }
    }

    for (auto& line : lines)
    {
        if (line.empty())
        {
            continue;
        }
        result += line + "\n";
    }
    if (!result.empty() && result.back() == '\n')
    {
        result.pop_back();    //去掉最后一个换行符
    }

    // 删除转换后不再被 goto 引用的孤立标签
    {
        std::map<std::string, std::string::size_type> unused_labels;
        std::string::size_type i = 0;
        while (i < result.size())
        {
            auto pos = result.find("::", i);
            if (pos == std::string::npos)
            {
                break;
            }
            auto pos_end = result.find("::", pos + 2);
            if (pos_end == std::string::npos)
            {
                break;
            }
            std::string label = result.substr(pos + 2, pos_end - pos - 2);
            if (!result.contains("goto " + label))
            {
                unused_labels[label] = i;
            }
            i = pos_end + 2;
        }
        unused_labels.erase("exit");    // 不处理结束标签
        for (const auto& [label, _] : unused_labels)
        {
            auto pos_label = result.find("::" + label + "::");
            if (pos_label != std::string::npos)
            {
                result.erase(pos_label, label.size() + 4);
            }
        }
    }

    strfunc::replaceAllSubStringRef(result, "\n\n", "\n");
    //filefunc::writeStringToFile(result, "out.lua");
    return result;
}
