
#include <iostream>
#include <string>
#include <vector>

#include "filefunc.h"
#include "GrpIdxFile.h"
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

struct Instruct
{
    int id;
    std::string name;
    int length;
    int is_bool;
    int jump1;
    int jump2;
};

std::map<int, Instruct> ins_;
std::vector<std::string> talks_;

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

void init_ins(std::string ini_file, std::string path)
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

    if (filefunc::fileExist(path + "/talkutf8.txt"))
    {
        talks_ = get_talk_utf8(path + "/talkutf8.txt");
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

std::string transk(const std::vector<int> e)
{
    int i = 0, i_line = 0;
    std::map<double, std::string> lines;    //索引为指令的位置，方便处理偏移
    while (i < e.size())
    {
        i_line++;
        int ins_id = e[i];
        auto& ins = ins_[ins_id];

        std::string str = ins.name;

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
    content += "::exit::";

    return content;
}