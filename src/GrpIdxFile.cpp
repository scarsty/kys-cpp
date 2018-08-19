#include "GrpIdxFile.h"
#include "File.h"

char* GrpIdxFile::getIdxContent(std::string filename_idx, std::string filename_grp, std::vector<int>* offset, std::vector<int>* length)
{
    std::vector<int> Ridx;
    File::readFileToVector(filename_idx, Ridx);

    offset->resize(Ridx.size() + 1);
    length->resize(Ridx.size());
    offset->at(0) = 0;
    for (int i = 0; i < Ridx.size(); i++)
    {
        (*offset)[i + 1] = Ridx[i];
        (*length)[i] = (*offset)[i + 1] - (*offset)[i];
    }
    int total_length = offset->back();

    auto Rgrp = new char[total_length];
    File::readFile(filename_grp, Rgrp, total_length);
    return Rgrp;
}
