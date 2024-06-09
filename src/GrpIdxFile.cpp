#include "GrpIdxFile.h"
#include "filefunc.h"

std::string GrpIdxFile::getIdxContent(const std::string& filename_idx, const std::string& filename_grp, std::vector<int>* offset, std::vector<int>* length)
{
    std::vector<int> Ridx;
    filefunc::readFileToVector(filename_idx, Ridx);

    offset->resize(Ridx.size() + 1);
    length->resize(Ridx.size());
    offset->at(0) = 0;
    for (int i = 0; i < Ridx.size(); i++)
    {
        (*offset)[i + 1] = Ridx[i];
        (*length)[i] = (*offset)[i + 1] - (*offset)[i];
    }
    int total_length = offset->back();

    std::string Rgrp;
    Rgrp.resize(total_length);
    readFile(filename_grp, (void*)Rgrp.data(), total_length);
    return Rgrp;
}

int GrpIdxFile::readFile(const std::string& filename, void* s, int length)
{
    FILE* fp = fopen(filename.c_str(), "rb");
    if (!fp)
    {
        //fprintf(stderr, "Cannot open file %s\n", filename.c_str());
        return 0;
    }
    int r = fread(s, 1, length, fp);
    fclose(fp);
    return r;
}

int GrpIdxFile::writeFile(const std::string& filename, void* s, int length)
{
    FILE* fp = fopen(filename.c_str(), "wb");
    if (!fp)
    {
        //fprintf(stderr, "Cannot write file %s\n", filename.c_str());
        return 0;
    }
    fseek(fp, 0, 0);
    fwrite(s, 1, length, fp);
    fclose(fp);
    return length;
}
