#pragma once
#include <string>
#include <vector>

class GrpIdxFile
{
public:
    static std::string getIdxContent(const std::string& filename_idx, const std::string& filename_grp, std::vector<int>* offset, std::vector<int>* length);
};
