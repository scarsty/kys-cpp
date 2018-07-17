#pragma once
#include <string>
#include <vector>

class GrpIdxFile
{
public:
    static std::vector<char> getIdxContent(std::string filename_idx, std::string filename_grp, std::vector<int>* offset, std::vector<int>* length);
};
