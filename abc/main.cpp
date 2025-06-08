
#include "abc.h"
#include "png_offset.h"
#include "filefunc.h"


void test_png_offset()
{
    //试验将PNG图片的偏移量写入到文件中

    write_png_offset(R"(D:\~temp\data\1.png)", 100, 200);

    int x = 0, y = 0;
    std::string content = filefunc::readFileToString(R"(D:\~temp\data\1.png)");
    if (read_png_offset(R"(D:\~temp\data\1.png)", x, y))
    {
        printf("offset: %d, %d\n", x, y);
    }
    else
    {
        printf("Failed to get PNG offset.\n");
    }
}


int main()
{
#ifdef _WIN32
    //system("chcp 65001");
#endif
    std::string path = "./";
    check_script(path +"script/event");
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