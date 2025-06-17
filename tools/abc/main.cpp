
#include "abc.h"
#include "filefunc.h"
#include "png_offset.h"



int main()
{
#ifdef _WIN32
    //system("chcp 65001");
#endif
    std::string path = "./";
    check_script(R"(D:\kys-all\trans50/event)");
    //check_fight_frame(path + "resource/fight", 0);
    //trans_fight_frame(path + "resource/fight");

    //trans_bin_list(path + "list/levelup.bin", path + "list/levelup.txt");
    //trans_bin_list(path + "list/leave.bin", path + "list/leave.txt");
    //initDBFieldInfo();
    //expandR(path + "save/ranger.idx", path + "save/ranger.grp", 0);
    //expandR(path + "save/ranger.idx", path + "save/r1.grp", 1);
    //expandR(path + "save/ranger.idx", path + "save/r2.grp", 2);
    //expandR(path + "save/ranger.idx", path + "save/r3.grp", 3);
    //expandR(path + "save/ranger.idx", path + "save/r4.grp", 4);
    //expandR(path + "save/ranger.idx", path + "save/r5.grp", 5);

    //expandR(path + "save/ranger.idx", path + "save/ranger.grp", 0, false, true);

    //make_heads(R"(D:\_sty_bak\kys-all\转换至kys-cpp工具\头像)");

    //combine_ka(path + "resource/wmap/index.ka", path + "resource/smap/index.ka");
    //split_eft_file(path + "resource/eft", path + "list/effect.bin");

    //test_png_offset();

    return 0;
}