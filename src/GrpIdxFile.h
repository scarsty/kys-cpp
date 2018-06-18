#pragma once
#include <vector>

class GrpIdxFile
{
public:
    static char* getIdxContent(std::string filename_idx, std::string filename_grp, std::vector<int>* offset, std::vector<int>* length);
};
