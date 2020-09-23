
//一些辅助的功能
//一些常数的设置比较不合理，建议以调试模式手动执行

#include "File.h"
#include "GrpIdxFile.h"
#include "TypesABC.h"
#include "convert.h"

#define FMT_HEADER_ONLY
#include "fmt/format.h"

//转换二进制文件为文本
void trans_bin_list(std::string in, std::string out)
{
    std::vector<int16_t> leave_list;
    File::readFileToVector(in, leave_list);

    std::string s;
    for (auto a : leave_list)
    {
        s += fmt::format("{}, ", a);
    }
    convert::writeStringToFile(s, out);
}

//导出战斗帧数为文本
void trans_fight_frame()
{
    for (int i = 0; i <= 300; i++)
    {
        std::string path = fmt::format("../game/resource/fight/fight{:03}", i);
        std::vector<int16_t> frame;
        std::string filename = path + "/fightframe.ka";
        if (File::fileExist(filename))
        {
            File::readFileToVector(path + "/fightframe.ka", frame);
            std::string content;
            fmt::print("role {}\n", i);
            for (int j = 0; j < 5; j++)
            {
                if (frame[j] > 0)
                {
                    fmt::print("{}, {}\n", j, frame[j]);
                    content += fmt::format(content.c_str(), "{}, {}\r\n", j, frame[j]);
                }
            }
            convert::writeStringToFile(content, path + "/fightframe.txt");
        }
    }
}

//扩展存档，将短整数扩展为int32
int expandR(std::string idx, std::string grp, bool ranger = true)
{
    if (!File::fileExist(grp) || !File::fileExist(idx))
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

    if (ranger)
    {
        std::vector<RoleSave1> roles_mem_;
        std::vector<MagicSave1> magics_mem_;
        std::vector<ItemSave1> items_mem_;
        std::vector<SubMapInfoSave1> submap_infos_mem_;
        File::readDataToVector(rgrp1 + offset1[1], length1[1], roles_mem_);
        File::readDataToVector(rgrp1 + offset1[2], length1[2], items_mem_);
        File::readDataToVector(rgrp1 + offset1[3], length1[3], submap_infos_mem_);
        File::readDataToVector(rgrp1 + offset1[4], length1[4], magics_mem_);

        std::vector<RoleSave> roles;
        std::vector<MagicSave> magics;
        std::vector<ItemSave> items;
        std::vector<SubMapInfoSave> submap_infos;
        File::readDataToVector(rgrp2 + offset2[1], length2[1], roles);
        File::readDataToVector(rgrp2 + offset2[2], length2[2], items);
        File::readDataToVector(rgrp2 + offset2[3], length2[3], submap_infos);
        File::readDataToVector(rgrp2 + offset2[4], length2[4], magics);
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
        File::writeVectorToData(rgrp2 + offset2[1], length2[1], roles, sizeof(RoleSave));
        File::writeVectorToData(rgrp2 + offset2[2], length2[2], items, sizeof(ItemSave));
        File::writeVectorToData(rgrp2 + offset2[3], length2[3], submap_infos, sizeof(SubMapInfoSave));
        File::writeVectorToData(rgrp2 + offset2[4], length2[4], magics, sizeof(MagicSave));
    }
    s32[1]--;    //submap scene id
    File::writeFile(grp + "32", rgrp2, len * 2);
    File::writeFile(idx + "32", &offset2[1], 4 * offset2.size() - 4);
    delete rgrp1;
    delete rgrp2;
    fmt::print("trans {} end\n", grp.c_str());
    return 0;
}

int main()
{
    trans_bin_list("../game/binlist/levelup.bin", "../game/list/levelup.txt");
    trans_bin_list("../game/binlist/leave.bin", "../game/list/leave.txt");
    trans_fight_frame();

    expandR("../game/save/ranger.idx", "../game/save/ranger.grp");

    for (int i = 1; i <= 20; i++)
    {
        std::string grp = "../game/save/r" + std::to_string(i) + ".grp";
        expandR("../game/save/ranger.idx", grp);
        grp = "../game/save/s" + std::to_string(i) + ".grp";
        expandR("../game/save/allsin.idx", grp, false);
        grp = "../game/save/d" + std::to_string(i) + ".grp";
        expandR("../game/save/alldef.idx", grp, false);
    }
    expandR("../game/resource/kdef.idx", "../game/resource/kdef.grp", false);
    expandR("../game/resource/warfld.idx", "../game/resource/warfld.grp", false);

    return 0;
}
