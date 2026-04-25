
// makeRanger.cpp — Excel (.xlsx) 转换为 ranger.grp / ranger.idx 工具
//
// Excel 表格结构：
//   Sheet 0：基本信息（BaseInfo）— B 列为字段名:size，C 列为值
//   Sheet 1：角色表（RoleSave）
//   Sheet 2：物品表（ItemSave）
//   Sheet 3：子场景信息表（SubMapInfoSave）
//   Sheet 4：武功表（MagicSave）
//   Sheet 5：商店表（ShopSave）
//
// 输出文件： ranger.grp（连续二进制数据） + ranger.idx（各 sheet 起始偏移表）
// 用法：  makeRanger <filename.xlsx>

#include <iostream>
#include <print>
#include <algorithm> // Add this include at the top of the file for std::min

#include "xlnt/xlnt.hpp"

// 解析一个工作表并将数据写入缓冲区。
// 表格约定：
//   第 1 行：表头名称（忽略）
//   第 2 行：字段描述，格式为 "name:size" 或 "name:string:size"
//   第 3+ 行：实际数据
// type==0：整数（下层用 memcpy 按 len 字节写入），type==1：字符串字段（固定长度 len）
// 返回本次写入的总字节数
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

    idx.push_back(p - buffer.data());    // Sheet 0 结束位置 = Sheet 1 起始偏移

    // Sheet 1-5 逐一转换，idx 积累存档各表的各层累起始地址
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
    size_t dataSize = p - buffer.data();
    size_t written = fwrite(buffer.data(), 1, dataSize, fp);
    fclose(fp);
    if (written != dataSize)
    {
        std::print("Failed to write ranger.grp (wrote {}/{} bytes)\n", written, dataSize);
        return 1;
    }

    FILE* fp2 = fopen("ranger.idx", "wb+");
    if (fp2 == NULL)
    {
        std::print("Failed to open file for writing\n");
        return 1;
    }
    size_t written2 = fwrite(idx.data(), sizeof(int), idx.size(), fp2);
    fclose(fp2);
    if (written2 != idx.size())
    {
        std::print("Failed to write ranger.idx (wrote {}/{} items)\n", written2, idx.size());
        return 1;
    }

    return 0;



}

