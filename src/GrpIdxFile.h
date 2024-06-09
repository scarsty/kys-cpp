#pragma once
#include <string>
#include <vector>

class GrpIdxFile
{
public:
    static std::string getIdxContent(const std::string& filename_idx, const std::string& filename_grp, std::vector<int>* offset, std::vector<int>* length);
    static int readFile(const std::string& filename, void* s, int length);
    static int writeFile(const std::string& filename, void* s, int length);
};
