#include "File.h"
#include "GrpIdxFile.h"

std::vector<char> GrpIdxFile::getIdxContent(std::string filename_idx, std::string filename_grp, std::vector<int>* offset, std::vector<int>* length)
{
    auto rIdxBytes = File::readFileVecChar(filename_idx);
    offset->resize(rIdxBytes.size() / 4 + 1);
    length->resize(rIdxBytes.size() / 4);
    offset->at(0) = 0;
    for (int i = 0; i < rIdxBytes.size() / 4; i++)
    {
        //转换为int
        memcpy(&(*offset)[i + 1], rIdxBytes.data() + i * sizeof(int), sizeof(int));
        (*length)[i] = (*offset)[i + 1] - (*offset)[i];
    }
    int total_length = offset->back();

	std::vector<char> Rgrp(total_length);
    File::readFile(filename_grp, Rgrp.data(), total_length);
    return Rgrp;
}
