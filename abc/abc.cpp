
//一些辅助的功能
//一些常数的设置比较不合理，建议以调试模式手动执行

#include "GameUtil.h"
#include "GrpIdxFile.h"
#include "OpenCCConverter.h"
#include "PotConv.h"
#include "Save.h"
#include "TypesABC.h"
#include "filefunc.h"
#include "fmt1.h"
#include "strfunc.h"

//转换二进制文件为文本
void trans_bin_list(std::string in, std::string out)
{
    std::vector<int16_t> leave_list;
    filefunc::readFileToVector(in, leave_list);

    std::string s;
    for (auto a : leave_list)
    {
        s += fmt1::format("{}, ", a);
    }
    filefunc::writeStringToFile(s, out);
}

//导出战斗帧数为文本
void trans_fight_frame(std::string path0)
{
    for (int i = 0; i <= 300; i++)
    {
        std::string path = fmt1::format("{}/fight{:03}", path0, i);
        std::vector<int16_t> frame;
        std::string filename = path + "/fightframe.ka";
        if (filefunc::fileExist(filename))
        {
            filefunc::readFileToVector(path + "/fightframe.ka", frame);
            std::string content;
            fmt1::print("role {}\n", i);
            for (int j = 0; j < 5; j++)
            {
                if (frame[j] > 0)
                {
                    fmt1::print("{}, {}\n", j, frame[j]);
                    content += fmt1::format("{}, {}\n", j, frame[j]);
                }
            }
            filefunc::writeStringToFile(content, path + "/fightframe.txt");
        }
    }
}

//扩展存档，将短整数扩展为int32
//最后一个参数：帧数需从之前存档格式获取
int expandR(std::string idx, std::string grp, bool ranger = true, bool make_fightframe = false)
{
    if (!filefunc::fileExist(grp) || !filefunc::fileExist(idx))
    {
        return -1;
    }

    std::vector<int> offset1, length1, offset2, length2;
    auto rgrp1_str = GrpIdxFile::getIdxContent(idx, grp, &offset1, &length1);
    auto rgrp1 = rgrp1_str.c_str();
    offset2 = offset1;
    length2 = length1;
    for (auto& i : offset2)
    {
        i *= 2;
    }
    for (auto& i : length2)
    {
        i *= 2;
    }

    int len = offset1.back();
    auto rgrp2 = new char[len * 2];

    auto s16 = (int16_t*)rgrp1;
    auto s32 = (int*)rgrp2;
    for (int i = 0; i < len / 2; i++)
    {
        s32[i] = s16[i];
    }

    if (ranger || make_fightframe)
    {
        std::vector<RoleSave1> roles_mem_;
        std::vector<MagicSave1> magics_mem_;
        std::vector<ItemSave1> items_mem_;
        std::vector<SubMapInfoSave1> submap_infos_mem_;
        filefunc::readDataToVector(rgrp1 + offset1[1], length1[1], roles_mem_);
        if (make_fightframe)
        {
            for (auto it = roles_mem_.rbegin(); it != roles_mem_.rend(); it++)
            {
                auto r = *it;
                std::string content;
                fmt1::print("role {}\n", r.HeadID);
                for (int j = 0; j < 5; j++)
                {
                    if (r.Frame[j] > 0)
                    {
                        fmt1::print("{}, {}\n", j, r.Frame[j]);
                        content += fmt1::format("{}, {}\n", j, r.Frame[j]);
                    }
                }
                if (!content.empty())
                {
                    auto p = filefunc::getParentPath(idx);
                    p = filefunc::getParentPath(p);
                    auto f = fmt1::format("{}/resource/fight/fight{:03}/fightframe.txt", p, r.HeadID);
                    filefunc::writeStringToFile(content, f);
                }
            }
            if (!ranger) { return 1; }
        }

        filefunc::readDataToVector(rgrp1 + offset1[2], length1[2], items_mem_);
        filefunc::readDataToVector(rgrp1 + offset1[3], length1[3], submap_infos_mem_);
        filefunc::readDataToVector(rgrp1 + offset1[4], length1[4], magics_mem_);

        std::vector<RoleSave> roles;
        std::vector<MagicSave> magics;
        std::vector<ItemSave> items;
        std::vector<SubMapInfoSave> submap_infos;
        filefunc::readDataToVector(rgrp2 + offset2[1], length2[1], roles);
        filefunc::readDataToVector(rgrp2 + offset2[2], length2[2], items);
        filefunc::readDataToVector(rgrp2 + offset2[3], length2[3], submap_infos);
        filefunc::readDataToVector(rgrp2 + offset2[4], length2[4], magics);
        for (int i = 0; i < roles.size(); i++)
        {
            memset(roles[i].Name, 0, sizeof(roles[i].Name));
            memcpy(roles[i].Name, roles_mem_[i].Name, sizeof(roles_mem_[i].Name));
            memset(roles[i].Nick, 0, sizeof(roles[i].Nick));
            memcpy(roles[i].Nick, roles_mem_[i].Nick, sizeof(roles_mem_[i].Nick));
        }
        for (int i = 0; i < items.size(); i++)
        {
            memset(items[i].Name, 0, sizeof(items[i].Name));
            memcpy(items[i].Name, items_mem_[i].Name, sizeof(items_mem_[i].Name));
            memset(items[i].Introduction, 0, sizeof(items[i].Introduction));
            memcpy(items[i].Introduction, items_mem_[i].Introduction, sizeof(items_mem_[i].Introduction));
        }
        for (int i = 0; i < magics.size(); i++)
        {
            memset(magics[i].Name, 0, sizeof(magics[i].Name));
            memcpy(magics[i].Name, magics_mem_[i].Name, sizeof(magics_mem_[i].Name));
        }
        for (int i = 0; i < submap_infos.size(); i++)
        {
            memset(submap_infos[i].Name, 0, sizeof(submap_infos[i].Name));
            memcpy(submap_infos[i].Name, submap_infos_mem_[i].Name, sizeof(submap_infos_mem_[i].Name));
        }
        filefunc::writeVectorToData(rgrp2 + offset2[1], length2[1], roles, sizeof(RoleSave));
        filefunc::writeVectorToData(rgrp2 + offset2[2], length2[2], items, sizeof(ItemSave));
        filefunc::writeVectorToData(rgrp2 + offset2[3], length2[3], submap_infos, sizeof(SubMapInfoSave));
        filefunc::writeVectorToData(rgrp2 + offset2[4], length2[4], magics, sizeof(MagicSave));
    }
    s32[1]--;    //submap scene id
    GrpIdxFile::writeFile(grp + "32", rgrp2, len * 2);
    GrpIdxFile::writeFile(idx + "32", &offset2[1], 4 * offset2.size() - 4);
    //delete rgrp1;
    delete rgrp2;

    fmt1::print("trans {} end\n", grp.c_str());
    return 0;
}

//将in的非零数据转到out
void combine_ka(std::string in, std::string out)
{
    std::vector<int16_t> in1, out1;
    filefunc::readFileToVector(in, in1);
    filefunc::readFileToVector(out, out1);
    std::string s;
    int i = 0;
    for (int i = 0; i < out1.size(); i += 2)
    {
        if (i < in1.size() && out1[i] == 0 && out1[i + 1] == 0)
        {
            out1[i] = in1[i];
            out1[i + 1] = in1[i + 1];
            fmt1::print("{}, ", i / 2);
        }
    }
    GrpIdxFile::writeFile(out, out1.data(), out1.size() * 2);
    //filefunc::writeStringToFile(s, out);
}

//验证战斗帧数的正确性
void check_fight_frame(std::string path, int repair = 0)
{
    for (int i = 0; i < 500; i++)
    {
        auto path1 = fmt1::format("{}/fight{:03}", path, i);
        if (filefunc::pathExist(path1))
        {
            auto files = filefunc::getFilesInPath(path1, 0);
            int count = files.size() - 3;
            int sum = 0;
            auto filename = path1 + "/fightframe.txt";
            auto numbers = strfunc::findNumbers<int>(filefunc::readFileToString(filename));
            for (int i = 0; i < numbers.size(); i += 2)
            {
                sum += numbers[i + 1];
            }
            if (sum * 4 != count)
            {
                fmt1::print("{}\n", path1);
                if (repair)
                {
                    if (numbers.size() <= 2)
                    {
                        auto str = fmt1::format("{}, {}", numbers[0], count / 4);
                        filefunc::writeStringToFile(str, filename);
                    }
                }
            }
        }
    }
}

//检查3号指令的最后3个参数正确性
void check_script(std::string path)
{
    auto files = filefunc::getFilesInPath(path, 0);
    for (auto& f : files)
    {
        bool repair = false;
        auto lines = strfunc::splitString(filefunc::readFileToString(path + "/" + f), "\n", false);
        for (auto& line : lines)
        {
            auto num = strfunc::findNumbers<int>(line);
            if (num.size() >= 13 && line.find("--") != 0)
            {
                if (num[0] == 3)
                {
                    if (num[1] > 0 && num[12] >= 0 && num[13] >= 0)
                    {
                        fmt1::print("{}\n", line);
                        auto strs = strfunc::splitString(line, ",", false);
                        strs[10] = "-2";
                        strs[11] = "-2";
                        strs[12][0] = '2';
                        strs[12] = "-" + strs[12];
                        std::string new_str;
                        for (auto& s : strs)
                        {
                            new_str += s + ",";
                        }
                        new_str.pop_back();
                        fmt1::print("{}\n", new_str);
                        line = "--repair" + line + "\n" + new_str;
                        repair = true;
                    }
                    if (num[12] < 0 && num[13] < 0)
                    {
                        //fmt1::print("{}\n", line);
                    }
                }
            }
        }
        if (repair)
        {
            std::string new_str;
            for (auto& s : lines)
            {
                new_str += s + "\n";
            }
            filefunc::writeStringToFile(new_str, path + "/" + f);
        }
    }
}

//重新产生头像
void make_heads(std::string path)
{
    auto h_lib = filefunc::getFilesInPath(path);
    Save::getInstance()->loadR(0);
    OpenCCConverter::getInstance()->set("cc/t2s.json");
    for (auto r : Save::getInstance()->getRoles())
    {
        std::string name = r->Name;
        name = OpenCCConverter::getInstance()->UTF8s2t(name);
        name = PotConv::utf8tocp936(name);
        for (auto& h : h_lib)
        {
            if (h.find(name) == 0)
            {
                fmt1::print("{} {}\n", r->HeadID, h);
                auto file_src = path + "\\" + h;
                auto file_dst = fmt1::format("..\\game\\resource\\head1\\{}.png", r->HeadID);
                auto cmd = "copy " + file_src + " " + file_dst;
                system(cmd.c_str());
                break;
            }
        }
    }
}

int main()
{
#ifdef _WIN32
    //system("chcp 65001");
#endif
    std::string path = "../game/";
    //check_script(path +"script/oldevent");
    //check_fight_frame(path +"resource/fight", 1);
    //trans_fight_frame();

    trans_bin_list(path + "list/levelup.bin", path + "list/levelup.txt");
    trans_bin_list(path + "list/leave.bin", path + "list/leave.txt");

    expandR(path + "save/ranger.idx", path + "save/ranger.grp");
    expandR(path + "save/ranger.idx", path + "save/r1.grp");
    expandR(path + "save/ranger.idx", path + "save/r2.grp");
    expandR(path + "save/ranger.idx", path + "save/r3.grp");
    expandR(path + "save/ranger.idx", path + "save/r4.grp");
    expandR(path + "save/ranger.idx", path + "save/r5.grp");

    GameUtil::PATH() = path;
    Save::getInstance()->loadR(0);
    Save::getInstance()->saveRToDB(0);
    expandR(path + "save/ranger.idx", path + "save/ranger.grp", false, true);

    make_heads(R"(D:\_sty_bak\kys-all\转换至kys-cpp工具\头像)");

    combine_ka(path + "resource/wmap/index.ka", path + "resource/smap/index.ka");

    return 0;
}
