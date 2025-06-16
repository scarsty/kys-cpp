

#include <iostream>
#include <print>
#include <algorithm> // Add this include at the top of the file for std::min

#include "xlnt/xlnt.hpp"

int trans(const xlnt::worksheet& ws, uint8_t*& p)
{
    int total = 0;
    std::vector<int> lens, type;
    int i = 0;
    for (auto row : ws.rows())
    {
        int len = 0;

        if (i == 1)
        {
            for (auto cell : row)
            {
                auto str = cell.to_string();
                auto pos = str.find_last_of(":");
                if (pos != std::string::npos)
                {
                    auto str1 = str.substr(pos + 1);
                    len = atoi(str1.c_str());
                    lens.push_back(len);
                    if (str.contains("string"))
                    {
                        type.push_back(1);
                    }
                    else
                    {
                        type.push_back(0);
                    }
                }
            }
        }
        if (i >= 2)
        {
            int c = 0;
            for (auto cell : row)
            {
                if (cell.has_value())
                {
                    len = lens[c];
                    if (type[c] == 0)
                    {
                        auto str = cell.to_string();
                        int value = atoi(str.c_str());
                        memcpy(p, &value, len);
                        p += len;
                        total += len;
                    }
                    else if (type[c] == 1)
                    {
                        std::string str = cell.to_string();
                        memcpy(p, str.c_str(), std::min(len, int(str.length())));
                        p += len;
                        total += len;
                    }
                }
                c++;
            }
        }
        i++;
    }
    return total;
}


int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::print("Usage: makeRanger <filename>\n");
        return 1;
    }

    std::string filename = argv[1];
    std::print("Processing file: {}\n", filename);

    xlnt::workbook wb;
    wb.load(filename);

    std::vector<uint8_t> buffer(2000000);
    uint8_t* p = buffer.data();

    //基本数据
    auto ws = wb.sheet_by_index(0);
    std::vector<int> idx;
    for (auto row : ws.rows())
    {
        int c = 0;
        int len = 0;
        for (auto cell : row)
        {
            if (c == 1 && cell.has_value())
            {
                auto str = cell.to_string();
                auto pos = str.find_last_of(":");
                if (pos != std::string::npos)
                {
                    str = str.substr(pos + 1);
                    len = atoi(str.c_str());
                }
            }
            if (c == 2 && cell.has_value())
            {
                auto str = cell.to_string();
                int value = atoi(str.c_str());
                if (len > 0)
                {
                    memcpy(p, &value, len);
                    p += len;
                }
            }
            c++;
        }
    }

    idx.push_back(p - buffer.data());

    idx.push_back(trans(wb.sheet_by_index(1), p) + idx.back());
    idx.push_back(trans(wb.sheet_by_index(2), p) + idx.back());
    idx.push_back(trans(wb.sheet_by_index(3), p) + idx.back());
    idx.push_back(trans(wb.sheet_by_index(4), p) + idx.back());
    idx.push_back(trans(wb.sheet_by_index(5), p) + idx.back());



    FILE* fp = fopen("ranger.grp", "wb+");
    if (fp == NULL)
    {
        std::print("Failed to open file for writing\n");
        return 1;
    }
    size_t written = fwrite(buffer.data(), 1, p - buffer.data(), fp);
    fclose(fp);

    FILE* fp2 = fopen("ranger.idx", "wb+");
    if (fp2 == NULL)
    {
        std::print("Failed to open file for writing\n");
        return 1;
    }
    size_t written2 = fwrite(idx.data(), sizeof(int), idx.size(), fp2);
    fclose(fp2);

    return 0;



}

