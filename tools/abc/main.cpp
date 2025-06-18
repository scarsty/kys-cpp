
#include "abc.h"
#include "cmdline.h"
#include "filefunc.h"
#include "png_offset.h"

#ifdef _MSC_VER
#include <windows.h>
#endif

int main()
{
    cmdline::parser cmd;

    cmd.add("save", '\0', "trans save files to 32 format and db");
    cmd.add("list", '\0', "trans bin list files to txt");
    cmd.add("trans-fightframe", '\0', "trans fightframe.ka to fightframe.txt");
    cmd.add("check-fightframe", '\0', "cheak fightframe is right");
    cmd.add("split-eft", '\0', "split eft file to seperate paths");
    cmd.add("combine-wmpsmp", '\0', "combine wmp to smp");

    cmd.add<std::string>("path", 'p', "resource path", false, "./");    

#ifdef _MSC_VER
    cmd.parse_check(GetCommandLineA());
#else
    cmd.parse_check(argc, argv);
#endif

    std::string path = cmd.get<std::string>("path");
    if (path.back() != '/')
    {
        path += '/';
    }

    if (cmd.exist("save"))
    {
        initDBFieldInfo();
        expandR(path + "/ranger.idx", path + "/ranger.grp", 0, path, true, false);
        expandR(path + "/ranger.idx", path + "/r1.grp", 1, path, true, false);
        expandR(path + "/ranger.idx", path + "/r2.grp", 2, path, true, false);
        expandR(path + "/ranger.idx", path + "/r3.grp", 3, path, true, false);
        expandR(path + "/ranger.idx", path + "/r4.grp", 4, path, true, false);
        expandR(path + "/ranger.idx", path + "/r5.grp", 5, path, true, false);

        expandR(path  + "/ranger.idx", path + "/ranger.grp", 0, path, false, true);    //只转换战斗帧数
    }

    if (cmd.exist("list"))
    {
        trans_bin_list(path + "/levelup.bin", path + "/levelup.txt");
        trans_bin_list(path + "/leave.bin", path + "/leave.txt");
    }

    if (cmd.exist("trans-fightframe"))
    {
        trans_fight_frame(path);
    }

    if (cmd.exist("check-fightframe"))
    {
        check_fight_frame(path, 0);
    }

    if (cmd.exist("split-eft"))
    {
        split_eft_file(path + "/eft", path + "/effect.bin");
    }


    if (cmd.exist("combine-wmpsmp"))
    {
        combine_image_path(path + "/wmap", path + "/smap");
        combine_ka(path + "/wmap/index.ka", path + "/smap/index.ka");
    }

    //check_script(R"(D:\kys-all\trans50/event)");
    
    //make_heads(R"(D:\_sty_bak\kys-all\转换至kys-cpp工具\头像)");

    //test_png_offset();

    return 0;
}