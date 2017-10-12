
//一些辅助的功能，例如将二进制文件转为文本文件等


#include "../src/File.h"
#include "../src/others/libconvert.h"
#include "Types2.h"
#include "../src/PotConv.h"

void trans_bin_list()
{
    std::vector<int16_t> leave_list;
    File::readFileToVector("work/game/binlist/levelup.bin", leave_list);

    for (auto a : leave_list)
    {
        printf("%d, ", a);
    }
}

void trans_fight_frame()
{
    for (int i = 0; i <= 109; i++)
    {
        std::string path = convert::formatString("work/game/resource/fight/fight%03d", i);
        std::vector<int16_t> frame;
        std::string filename = path + "/fightframe.ka";
        if (File::fileExist(filename))
        {
            File::readFileToVector(path + "/fightframe.ka", frame);
            std::string content;
            printf("role %d\n", i);
            for (int j = 0; j < 5; j++)
            {
                if (frame[j] > 0)
                {
                    printf("%d, %d\n", j, frame[j]);
                    convert::formatAppendString(content, "%d, %d\r\n", j, frame[j]);
                }
            }
            convert::writeStringToFile(content, path + "/fightframe.txt");
        }
    }
}





int expandR(std::string idx, std::string grp)
{
    if (!File::fileExist(grp) || !File::fileExist(idx))
    {
        return -1;
    }


    std::vector<int> offset, length;
    auto rgrp = File::getIdxContent(idx, grp, &offset, &length);

    int len = offset.back();
    int16_t* s16 = (int16_t*)rgrp;

    int* s32 = new int[len / 2];

    for (int i = 0; i < len / 2; i++)
    {
        s32[i] = s16[i];
    }

    std::vector<RoleSave1> roles_mem_;
    std::vector<MagicSave1> magics_mem_;
    std::vector<ItemSave1> items_mem_;
    std::vector<SubMapInfoSave1> submap_infos_mem_;
    File::readDataToVector(rgrp + offset[1], length[1], roles_mem_);
    File::readDataToVector(rgrp + offset[2], length[2], items_mem_);
    File::readDataToVector(rgrp + offset[3], length[3], submap_infos_mem_);
    File::readDataToVector(rgrp + offset[4], length[4], magics_mem_);

    delete rgrp;
    rgrp = (char*)s32;

    for (auto& i : offset) { i *= 2; }
    for (auto& i : length) { i *= 2; }

    std::vector<RoleSave> roles;
    std::vector<MagicSave> magics;
    std::vector<ItemSave> items;
    std::vector<SubMapInfoSave> submap_infos;

    File::readDataToVector(rgrp + offset[1], length[1], roles);
    File::readDataToVector(rgrp + offset[2], length[2], items);
    File::readDataToVector(rgrp + offset[3], length[3], submap_infos);
    File::readDataToVector(rgrp + offset[4], length[4], magics);

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
    File::writeVectorToData(rgrp + offset[1], length[1], roles, sizeof(RoleSave));
    File::writeVectorToData(rgrp + offset[2], length[2], items, sizeof(ItemSave));
    File::writeVectorToData(rgrp + offset[3], length[3], submap_infos, sizeof(SubMapInfoSave));
    File::writeVectorToData(rgrp + offset[4], length[4], magics, sizeof(MagicSave));

    File::writeFile(grp + "32", s32, len * 2);
    File::writeFile(idx + "32", &offset[1], 4 * offset.size() - 4);

    delete s32;
    return 0;
}



int main()
{
    //trans_bin_list();
    //trans_fight_frame();
    expandR("work/game/save/ranger.idx", "work/game/save/ranger.grp");
    expandR("work/game/save/ranger.idx", "work/game/save/r1.grp");
    expandR("work/game/save/ranger.idx", "work/game/save/r2.grp");
    expandR("work/game/save/ranger.idx", "work/game/save/r3.grp");
    expandR("work/game/save/ranger.idx", "work/game/save/r4.grp");
    expandR("work/game/save/ranger.idx", "work/game/save/r5.grp");
    expandR("work/game/save/ranger.idx", "work/game/save/r6.grp");
    return 0;
}

