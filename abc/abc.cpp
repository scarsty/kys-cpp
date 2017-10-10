
//一些辅助的功能，例如将二进制文件转为文本文件等


#include "../src/File.h"
#include "../src/others/libconvert.h"


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


int main()
{
    //trans_bin_list();
    trans_fight_frame();
    return 0;
}

