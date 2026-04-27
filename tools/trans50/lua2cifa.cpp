// lua2cifa.cpp — 将 Lua 事件脚本转换为 Cifa（类 C）格式
//
// 转换规则：
//   if COND then      ->  if (CIFA_COND)\n{
//   else              ->  }\nelse\n{
//   end; / end        ->  }
//   for VAR=A, B do   ->  for (int VAR = A; VAR <= B; VAR++)\n{
//   repeat            ->  do\n{
//   until COND;       ->  } while (NEGATE(CIFA_COND));
//   ::label::  + 之后对应的 if COND then goto label end;
//                     ->  do { ... } while (COND);
//   EXPR == false     ->  !EXPR
//   EXPR == true      ->  EXPR
//   ~=                ->  !=
//   or / and          ->  || / &&
//   -- comment        ->  // comment
//   !(a > b)          ->  a <= b  (及其他比较符对称翻转)

#include "strfunc.h"
#include <string>
#include <unordered_map>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// 辅助函数
// ─────────────────────────────────────────────────────────────────────────────

static std::string trim_str(const std::string& s)
{
    auto a = s.find_first_not_of(" \t");
    if (a == std::string::npos)
    {
        return "";
    }
    auto b = s.find_last_not_of(" \t");
    return s.substr(a, b - a + 1);
}

// 将条件取反，优先翻转比较运算符以获得最简形式：
//   !(a > b)  → a <= b
//   !(a >= b) → a < b
//   !(a < b)  → a >= b
//   !(a <= b) → a > b
//   !(a == b) → a != b
//   !(a != b) → a == b
//   其余情况  → !(cond)
static std::string negate_cond(const std::string& cond)
{
    // 按长度从长到短排列，避免 >= 被当成 > 提前匹配
    static const std::pair<std::string, std::string> ops[] = {
        { "!=", "==" }, { ">=", "<" }, { "<=", ">" }, { "==", "!=" }, { ">", "<=" }, { "<", ">=" }
    };
    int paren = 0;
    bool in_str = false;
    char str_ch = 0;
    for (size_t i = 0; i < cond.size(); i++)
    {
        char c = cond[i];
        if (in_str)
        {
            if (c == str_ch)
            {
                in_str = false;
            }
            continue;
        }
        if (c == '"' || c == '\'')
        {
            in_str = true;
            str_ch = c;
            continue;
        }
        if (c == '(')
        {
            paren++;
            continue;
        }
        if (c == ')')
        {
            paren--;
            continue;
        }
        if (paren == 0)
        {
            for (auto& [from, to] : ops)
            {
                if (cond.size() - i >= from.size() && cond.substr(i, from.size()) == from)
                {
                    return cond.substr(0, i) + to + cond.substr(i + from.size());
                }
            }
        }
    }
    return "!(" + cond + ")";
}

// 将 Lua 条件取反（保留 Lua 语法，用于替换 jump_flag）：
//   x[1] == 32  →  x[1] ~= 32
//   x[1] ~= 32  →  x[1] == 32
//   其余情况    →  not (cond)
static std::string negate_cond_lua(const std::string& cond)
{
    static const std::pair<std::string, std::string> ops[] = {
        { "~=", "==" }, { ">=", "<" }, { "<=", ">" }, { "==", "~=" }, { ">", "<=" }, { "<", ">=" }
    };
    int paren = 0;
    bool in_str = false;
    char str_ch = 0;
    for (size_t i = 0; i < cond.size(); i++)
    {
        char c = cond[i];
        if (in_str)
        {
            if (c == str_ch)
            {
                in_str = false;
            }
            continue;
        }
        if (c == '"' || c == '\'')
        {
            in_str = true;
            str_ch = c;
            continue;
        }
        if (c == '(')
        {
            paren++;
            continue;
        }
        if (c == ')')
        {
            paren--;
            continue;
        }
        if (paren == 0)
        {
            for (auto& [from, to] : ops)
            {
                if (cond.size() - i >= from.size() && cond.substr(i, from.size()) == from)
                {
                    return cond.substr(0, i) + to + cond.substr(i + from.size());
                }
            }
        }
    }
    return "not (" + cond + ")";
}

// 将 Lua 条件表达式转换为 Cifa 条件（不含 "!(..."  的 negate）
static std::string lua_cond(const std::string& raw)
{
    std::string cond = trim_str(raw);

    // "not EXPR"
    if (cond.size() >= 4 && cond.substr(0, 4) == "not ")
    {
        return "!" + trim_str(cond.substr(4));
    }

    // "EXPR == false"
    const std::string suf_false = " == false";
    if (cond.size() > suf_false.size() && cond.substr(cond.size() - suf_false.size()) == suf_false)
    {
        std::string inner = trim_str(cond.substr(0, cond.size() - suf_false.size()));
        // 若内层含顶层比较符，用 negate_cond 翻转；否则直接加 !
        bool complex = false;
        int paren = 0;
        for (char c : inner)
        {
            if (c == '(')
            {
                paren++;
            }
            else if (c == ')')
            {
                paren--;
            }
            else if (paren == 0 && (c == '<' || c == '>' || c == '!'))
            {
                complex = true;
            }
        }
        if (complex)
        {
            return negate_cond(inner);
        }
        return "!" + inner;
    }

    // "EXPR == true"
    const std::string suf_true = " == true";
    if (cond.size() > suf_true.size() && cond.substr(cond.size() - suf_true.size()) == suf_true)
    {
        return trim_str(cond.substr(0, cond.size() - suf_true.size()));
    }

    // 替换 ~=
    strfunc::replaceAllSubStringRef(cond, "~=", "!=");
    return cond;
}

// ─────────────────────────────────────────────────────────────────────────────
// lua2cifa — 主转换函数
// ─────────────────────────────────────────────────────────────────────────────
std::string lua2cifa(std::string str)
{
    // 去除 UTF-8 BOM（EF BB BF）
    if (str.size() >= 3 && (unsigned char)str[0] == 0xEF && (unsigned char)str[1] == 0xBB && (unsigned char)str[2] == 0xBF)
    {
        str.erase(0, 3);
    }
    // 规范化行尾
    strfunc::replaceAllSubStringRef(str, "\r\n", "\n");
    strfunc::replaceAllSubStringRef(str, "\r", "\n");

    std::vector<std::string> lines = strfunc::splitString(str, "\n", false);

    // ─────────────────────────────────────────────────
    // 预处理 B：消解 jump_flag 中间变量
    //   "if COND then jump_flag = true; else jump_flag = false; end;"
    //   + 之后的 "if jump_flag == false then goto LABEL end;"
    //   → 将后者替换为 "if NEGCOND then goto LABEL end;"，前者行清空
    // ─────────────────────────────────────────────────
    for (int i = 0; i < (int)lines.size(); i++)
    {
        std::string t = trim_str(lines[i]);
        if (t.find("jump_flag = true") == std::string::npos || t.find("jump_flag = false") == std::string::npos || t.substr(0, 3) != "if ")
        {
            continue;
        }

        auto pos_then = t.find(" then ");
        if (pos_then == std::string::npos)
        {
            continue;
        }
        std::string jf_cond = trim_str(t.substr(3, pos_then - 3));

        for (int j = i + 1; j < (int)lines.size(); j++)
        {
            std::string t2 = trim_str(lines[j]);
            if (t2.find("jump_flag") == std::string::npos)
            {
                continue;
            }
            if (t2.substr(0, 3) != "if ")
            {
                continue;
            }

            if (t2.find("jump_flag == false") != std::string::npos)
            {
                strfunc::replaceAllSubStringRef(lines[j], "jump_flag == false", negate_cond_lua(jf_cond));
            }
            else
            {
                strfunc::replaceAllSubStringRef(lines[j], "jump_flag", jf_cond);
            }
            lines[i].clear();
            break;
        }
    }

    // ─────────────────────────────────────────────────
    // 预处理 C：消解 showmessage 后跟 jump_flag 的模式
    //   "x[28672] = showmessage(..., CODE);" 之后出现
    //   "if jump_flag == false then ..." → "if x[28672] != CODE then ..."
    //   "if jump_flag then ..."           → "if x[28672] == CODE then ..."
    // ─────────────────────────────────────────────────
    {
        std::string last_code;
        for (int i = 0; i < (int)lines.size(); i++)
        {
            std::string t = trim_str(lines[i]);
            if (t.find("showmessage(") != std::string::npos && t.find("x[28672]") != std::string::npos)
            {
                auto pos_close = t.rfind(')');
                auto pos_comma = t.rfind(',', pos_close);
                if (pos_comma != std::string::npos && pos_close > pos_comma)
                {
                    last_code = trim_str(t.substr(pos_comma + 1, pos_close - pos_comma - 1));
                }
            }
            if (t.find("jump_flag") != std::string::npos && t.find("jump_flag = true") == std::string::npos && t.find("jump_flag = false") == std::string::npos && !last_code.empty())
            {
                if (t.find("jump_flag == false") != std::string::npos)
                {
                    strfunc::replaceAllSubStringRef(lines[i], "jump_flag == false", "x[28672] != " + last_code);
                }
                else
                {
                    strfunc::replaceAllSubStringRef(lines[i], "jump_flag", "x[28672] == " + last_code);
                }
            }
        }
    }

    // ─────────────────────────────────────────────────
    // 第一遍：识别所有向后 goto 对
    //   ::labelN:: 在第 i 行 → 该行变为 "do {"（循环开始）
    //   if COND then goto labelN end; 在第 j 行（j>i）→ 该行变为 "} while (COND);"
    // ─────────────────────────────────────────────────
    std::unordered_map<int, bool> is_do_start;
    std::unordered_map<int, std::string> do_while_cond;

    for (int i = 0; i < (int)lines.size(); i++)
    {
        std::string t = trim_str(lines[i]);
        if (t.size() < 4 || t.substr(0, 2) != "::")
        {
            continue;
        }
        auto pos_end = t.find("::", 2);
        if (pos_end == std::string::npos)
        {
            continue;
        }
        std::string label = t.substr(2, pos_end - 2);
        if (label.empty())
        {
            continue;
        }

        std::string expected_goto = "goto " + label;
        for (int j = i + 1; j < (int)lines.size(); j++)
        {
            std::string t2 = trim_str(lines[j]);
            if (t2.find(expected_goto) == std::string::npos)
            {
                continue;
            }
            if (t2.size() < 3 || t2.substr(0, 3) != "if ")
            {
                continue;
            }
            auto pos_then = t2.find(" then ");
            if (pos_then == std::string::npos)
            {
                continue;
            }
            std::string cond = trim_str(t2.substr(3, pos_then - 3));
            is_do_start[i] = true;
            do_while_cond[j] = cond;
            break;
        }
    }

    // ─────────────────────────────────────────────────
    // 第二遍：逐行生成 Cifa 输出
    // ─────────────────────────────────────────────────
    std::vector<std::string> out;
    int depth = 0;

    auto indent = [&]() -> std::string
    {
        return std::string(depth * 4, ' ');
    };

    // 替换 Lua 逻辑运算符（or/and → ||/&&）和 ~=
    auto replace_operators = [](std::string& s)
    {
        strfunc::replaceAllSubStringRef(s, "~=", "!=");
        strfunc::replaceAllSubStringRef(s, " or ", " || ");
        strfunc::replaceAllSubStringRef(s, " and ", " && ");
    };

    // 将 Lua -- 注释替换为 // 注释（跳过字符串内部）
    auto replace_comment = [](std::string& s)
    {
        bool in_str = false;
        char str_ch = 0;
        for (size_t k = 0; k + 1 < s.size(); k++)
        {
            char c = s[k];
            if (in_str)
            {
                if (c == str_ch && (k == 0 || s[k - 1] != '\\'))
                {
                    in_str = false;
                }
            }
            else if (c == '"' || c == '\'')
            {
                in_str = true;
                str_ch = c;
            }
            else if (c == '-' && s[k + 1] == '-')
            {
                s[k] = '/';
                s[k + 1] = '/';
                break;
            }
        }
    };

    for (int i = 0; i < (int)lines.size(); i++)
    {
        std::string t = trim_str(lines[i]);
        if (t.empty())
        {
            continue;
        }

        // 向后 goto 标签行 → "do {"
        if (is_do_start.count(i))
        {
            out.push_back(indent() + "do");
            out.push_back(indent() + "{");
            depth++;
            continue;
        }

        // 向后 goto 结束行 → "} while (COND);"
        if (do_while_cond.count(i))
        {
            if (depth > 0)
            {
                depth--;
            }
            out.push_back(indent() + "} while (" + lua_cond(do_while_cond[i]) + ");");
            continue;
        }

        // "else"
        if (t == "else")
        {
            if (depth > 0)
            {
                depth--;
            }
            out.push_back(indent() + "}");
            out.push_back(indent() + "else");
            out.push_back(indent() + "{");
            depth++;
            continue;
        }

        // "end;" 或 "end"（关闭块）
        if (t == "end;" || t == "end")
        {
            if (depth > 0)
            {
                depth--;
            }
            out.push_back(indent() + "}");
            continue;
        }

        // "repeat"（Lua do-while 开始）
        if (t == "repeat")
        {
            out.push_back(indent() + "do");
            out.push_back(indent() + "{");
            depth++;
            continue;
        }

        // "until COND;"（Lua do-while 结束，条件取反）
        if (t.size() > 6 && t.substr(0, 6) == "until ")
        {
            std::string cond = trim_str(t.substr(6));
            if (!cond.empty() && cond.back() == ';')
            {
                cond.pop_back();
            }
            if (depth > 0)
            {
                depth--;
            }
            out.push_back(indent() + "} while (" + negate_cond(lua_cond(cond)) + ");");
            continue;
        }

        // "for VAR=A, B do"（数值 for 循环）
        if (t.size() > 7 && t.substr(0, 4) == "for " && t.substr(t.size() - 3) == " do")
        {
            std::string body = t.substr(4, t.size() - 7);
            auto eq_pos = body.find('=');
            auto comma_pos = body.find(',');
            if (eq_pos != std::string::npos && comma_pos != std::string::npos)
            {
                std::string var = trim_str(body.substr(0, eq_pos));
                std::string from = trim_str(body.substr(eq_pos + 1, comma_pos - eq_pos - 1));
                std::string to = trim_str(body.substr(comma_pos + 1));
                out.push_back(indent() + "for (int " + var + " = " + from + "; " + var + " <= " + to + "; " + var + "++)");
                out.push_back(indent() + "{");
                depth++;
                continue;
            }
        }

        // "if COND then"
        if (t.size() >= 8 && t.substr(0, 3) == "if " && t.substr(t.size() - 5) == " then")
        {
            std::string cond = trim_str(t.substr(3, t.size() - 8));
            out.push_back(indent() + "if (" + lua_cond(cond) + ")");
            out.push_back(indent() + "{");
            depth++;
            continue;
        }

        // 普通语句：替换运算符后直接输出
        std::string mod = t;
        replace_operators(mod);
        replace_comment(mod);
        out.push_back(indent() + mod);
    }

    std::string result;
    for (auto& l : out)
    {
        result += l + "\n";
    }
    if (!result.empty() && result.back() == '\n')
    {
        result.pop_back();
    }

    // 后处理：去除 Lua 标准库前缀（math./string. 在 Cifa 中直接使用函数名）
    strfunc::replaceAllSubStringRef(result, "math.random(", "random(");
    strfunc::replaceAllSubStringRef(result, "string.format(", "sprintf(");
    // string.rep(" ", EXPR) → sprintf("%-*s", EXPR, "")（生成 EXPR 个空格）
    {
        const std::string pat = "string.rep(\" \", ";
        size_t pos = 0;
        while ((pos = result.find(pat, pos)) != std::string::npos)
        {
            size_t arg_start = pos + pat.size();
            size_t arg_end = result.find(')', arg_start);
            if (arg_end != std::string::npos)
            {
                std::string expr = result.substr(arg_start, arg_end - arg_start);
                std::string repl = "sprintf(\"%-*s\", " + expr + ", \"\")";
                result.replace(pos, arg_end - pos + 1, repl);
                pos += repl.size();
            }
            else
            {
                pos += pat.size();
            }
        }
    }

    // 后处理：去除标识符与 '(' 之间的多余空格（如 "AskJoin ()" → "AskJoin()"）
    // 控制流关键字 if/while/for 后的空格保留
    for (size_t i = 1; i + 1 < result.size();)
    {
        if (result[i] == ' ' && result[i + 1] == '(' && (isalnum((unsigned char)result[i - 1]) || result[i - 1] == '_'))
        {
            size_t word_end = i;
            size_t word_start = word_end;
            while (word_start > 0 && (isalnum((unsigned char)result[word_start - 1]) || result[word_start - 1] == '_'))
            {
                word_start--;
            }
            std::string word = result.substr(word_start, word_end - word_start);
            if (word == "if" || word == "while" || word == "for")
            {
                i++;
                continue;
            }
            result.erase(i, 1);
        }
        else
        {
            i++;
        }
    }

    return result;
}
