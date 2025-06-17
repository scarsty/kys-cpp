
#include "filefunc.h"
#include <iostream>
#include <string>
#include <vector>

#include "GrpIdxFile.h"
#include "cmdline.h"

#ifdef _MSC_VER
#include <windows.h>
#endif

void init_ins(std::string ini_file, std::string path);
std::string transk(std::vector<int> e);
std::string trans50(std::string str, int refine);
void trans_talks(std::string talk_path, std::string coding);

int main(int argc, char* argv[])
{
    cmdline::parser cmd;
    cmd.add("talk", '\0', "trans talk to utf-8");
    cmd.add("kdef", '\0', "trans kdef to lua");
    cmd.add("50", '\0', "trans 50 in lua file");

    cmd.add<std::string>("in", 'i', "input path or file", false, ".");
    cmd.add<std::string>("out", 'o', "output path or file", false, ".");

    cmd.add<std::string>("talkpath", 't', "talk file for 50", false, "talkutf8.txt");
    cmd.add<std::string>("talkcoding", 'c', "talkcoding of grp", false, "cp950");
    cmd.add("refine", '\0', "simple refine when trans 50");

#ifdef _MSC_VER
    cmd.parse_check(GetCommandLineA());
#else
    cmd.parse_check(argc, argv);
#endif

    if (cmd.exist("talk"))
    {
        std::string talk_path = cmd.get<std::string>("in");
        trans_talks(talk_path, cmd.get<std::string>("talkcoding"));
    }

    if (cmd.exist("kdef"))
    {
        std::string path = cmd.get<std::string>("in");
        std::string path_out = cmd.get<std::string>("out");
        path_out += "/event";

        std::vector<int> offset, length;
        auto kdef_str = GrpIdxFile::getIdxContent(path + "/kdef.idx", path + "/kdef.grp", &offset, &length);
        std::vector<std::vector<int>> kdef_;
        kdef_.resize(length.size());
        for (int i = 0; i < length.size(); i++)
        {
            kdef_[i].resize(length[i] / sizeof(int16_t), -1);
            for (int k = 0; k < length[i] / sizeof(int16_t); k++)
            {
                kdef_[i][k] = *(int16_t*)(kdef_str.data() + offset[i] + k * 2);
            }
        }

        filefunc::makePath(path_out);
        init_ins("transk.ini", path);
        for (int i = 0; i < kdef_.size(); i++)
        {
            auto str = transk(kdef_[i]);
            //if (cmd.exist("50"))
            //{
            //    str = trans50(str);
            //}
            if (!str.empty())
            {
                filefunc::writeStringToFile(str, path_out + "/ka" + std::to_string(i) + ".lua");
            }
            printf("ka%d.lua\r", i);
        }
    }

    if (cmd.exist("50") && !cmd.exist("kdef"))
    {
        int refine = cmd.exist("refine") ? 1 : 0;
        init_ins("transk.ini", cmd.get<std::string>("talkpath"));
        if (cmd.exist("in"))
        {
            auto path = cmd.get<std::string>("in");
            std::string path_out = cmd.get<std::string>("out") + "/event50/";
            if (refine)
            {
                path_out = cmd.get<std::string>("out") + "/event50r/";
            }
            filefunc::makePath(path_out);
            if (filefunc::pathExist(path))
            {
                for (auto& file : filefunc::getFilesInPath(path, 0))
                {
                    if (filefunc::getFileExt(file) == "lua")
                    {
                        auto str = filefunc::readFileToString(path + "/" + file);
                        auto str1 = trans50(str, refine);
                        if (str1 != str)
                        {
                            filefunc::writeStringToFile(str1, path_out + file);
                        }
                    }
                    printf("%s\r", file.c_str());
                }
            }
            else if (filefunc::fileExist(path) && filefunc::getFileExt(path) == "lua")
            {
                auto str = filefunc::readFileToString(path);
                auto str1 = trans50(str, refine);
                if (str1 != str)
                {
                    filefunc::writeStringToFile(str1, cmd.get<std::string>("out") + "/" + filefunc::getFilenameWithoutPath(path));
                }
                else
                {
                    std::cerr << "The same after translation." << std::endl;
                }
            }
        }
    }

    return 0;
}