// ConsoleApplication1.cpp: 定义控制台应用程序的入口点。
//

#include "Timer.h"
#include "filefunc.h"
#include "strfunc.h"
#include <format>
#include <io.h>
#include <print>
#include <string>
#include <vector>
#ifdef _WIN32
#include <shlwapi.h>
#endif

#include "png.h"

extern std::vector<unsigned char> COL;

struct ImageData
{
    ImageData(int w, int h) :
        width(w), height(h)
    {
        setSize(w, h);
    }
    void setSize(int w, int h)
    {
        width = w;
        height = h;
        data.resize(w * h * 4);    // 4 channels for RGBA
    }
    void setPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        if (x < 0 || x >= width || y < 0 || y >= height)
        {
            return;
        }
        int index = (y * width + x) * 4;
        data[index] = r;
        data[index + 1] = g;
        data[index + 2] = b;
        data[index + 3] = a;
    }
    void setPixel(int x, int y, uint32_t c)
    {
        if (x < 0 || x >= width || y < 0 || y >= height)
        {
            return;
        }
        int index = (y * width + x) * 4;
        data[index] = (c >> 24) & 0xFF;        // Red
        data[index + 1] = (c >> 16) & 0xFF;    // Green
        data[index + 2] = (c >> 8) & 0xFF;     // Blue
        data[index + 3] = c & 0xFF;            // Alpha
    }
    void saveToFile(const std::string& filename)
    {
        png_structp png_ptr;
        png_infop info_ptr;
        FILE* png_file = fopen(filename.c_str(), "wb");
        if (!png_file)
        {
            return;
        }

        png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (png_ptr == NULL)
        {
            printf("ERROR:png_create_write_struct/n");
            fclose(png_file);
            return;
        }
        info_ptr = png_create_info_struct(png_ptr);
        if (info_ptr == NULL)
        {
            printf("ERROR:png_create_info_struct/n");
            png_destroy_write_struct(&png_ptr, NULL);
            return;
        }

        png_init_io(png_ptr, png_file);
        png_set_IHDR(
            png_ptr,
            info_ptr,
            width,
            height,
            8,
            PNG_COLOR_TYPE_RGBA,
            PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_BASE,
            PNG_FILTER_TYPE_BASE);

        png_write_info(png_ptr, info_ptr);
        png_set_packing(png_ptr);

        //这里就是图像数据了
        png_bytepp rows = (png_bytepp)png_malloc(png_ptr, height * sizeof(png_bytep));
        for (int i = 0; i < height; ++i)
        {
            rows[i] = (png_bytep)(data.data() + i * width * channels);
        }

        //png_set_compression_level(png_ptr, 9);
        png_write_image(png_ptr, rows);

        png_write_end(png_ptr, info_ptr);

        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(png_file);
    }
    ImageData& operator=(int i)
    {
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                setPixel(x, y, i);    // Set all pixels to the same color
            }
        }
        return *this;
    }

private:
    int width;
    int height;
    int channels = 4;    // RGBA
    std::vector<unsigned char> data;
};

int getFileLength(const std::string& filepath)
{
    FILE* file = fopen(filepath.c_str(), "rb");
    if (file)
    {
        int size = filelength(fileno(file));
        fclose(file);
        return size;
    }
    return 0;
}

int getFileRecord(const std::string& filepath)
{
    auto buffer = filefunc::readFile(filepath.c_str());
    int fileLength = buffer.size();
    int size = *(int*)(buffer.data() + fileLength - 4);
    return size;
}

void copyFile(std::string file1, std::string file2)
{
#ifdef _WIN32
    CopyFileA(file1.c_str(), file2.c_str(), FALSE);
#else
    char buf[BUFSIZ];
    size_t size;

    FILE* source = fopen(file1.c_str(), "rb");
    FILE* dest = fopen(file2.c_str(), "wb");

    // clean and more secure
    // feof(FILE* stream) returns non-zero if the end of file indicator for stream is set

    while (size = fread(buf, 1, BUFSIZ, source))
    {
        fwrite(buf, 1, size, dest);
    }

    fclose(source);
    fclose(dest);
#endif
}

int pair_grp_idx(std::string pathgrp, std::string pathidx)
{
    std::vector<std::string> grps, idxs;

    struct grpidx
    {
        std::string grpname;
        int record;
    };

    std::vector<grpidx> gggg;
    grps = filefunc::getFilesInPath(pathgrp);
    for (auto grp : grps)
    {
        auto grp0 = pathgrp + grp;
        //printf("%s, %d\n", grp.c_str(), getFileLength(grp0));
        gggg.push_back({ grp0, getFileLength(grp0) });
    }

    idxs = filefunc::getFilesInPath(pathidx);
    for (auto idx : idxs)
    {
        auto idx0 = pathidx + idx;
        for (auto& gi : gggg)
        {
            if (getFileRecord(idx0) == gi.record)
            {
                copyFile(idx0, filefunc::changeFileExt(gi.grpname, "idx"));
            }
        }
    }
    return 0;
}

int convert_grpidx_to_png(std::string idx_filename, std::string grp_filename, std::string path, bool slash = false)
{
    if (filefunc::fileExist(idx_filename) && filefunc::fileExist(grp_filename))
    {
        Timer timer;

        std::vector<int> idx;
        filefunc::readFileToVector(idx_filename, idx);
        idx.insert(idx.begin(), 0);
        auto grp = filefunc::readFile(grp_filename);

        std::print("{}: {} images\n", idx_filename, idx.size() - 1);
        std::vector<int16_t> offset((idx.size() - 1) * 2);

        filefunc::makePath(path);

        for (int m = 0; m < idx.size() - 1; m++)
        {
            if (idx[m + 1] <= 0 || idx[m + 1] - idx[m] <= 8)
            {
                continue;
            }
            int w = *(short*)&grp[idx[m] + 0];
            int h = *(short*)&grp[idx[m] + 2];
            int xoff = *(short*)&grp[idx[m] + 4];
            int yoff = *(short*)&grp[idx[m] + 6];
            offset[m * 2] = xoff;        // Save offsets for later use
            offset[m * 2 + 1] = yoff;    // Save offsets for later use
            if (w > 255 || h > 255)
            {
                continue;
            }
            if (w == 1 || h == 1)
            {
                continue;
            }
            int max_n = 1;
            auto col = COL;
            for (int n = 0; n < 9; n++)
            {
                if (n >= 1)
                {
                    auto change_col = [&col](int begin, int end)
                    {
                        auto temp0 = col[3 * end];
                        auto temp1 = col[3 * end + 1];
                        auto temp2 = col[3 * end + 2];
                        for (int j = end; j > begin; j--)
                        {
                            col[3 * j] = col[3 * (j - 1)];
                            col[3 * j + 1] = col[3 * (j - 1) + 1];
                            col[3 * j + 2] = col[3 * (j - 1) + 2];
                        }
                        col[3 * begin] = temp0;
                        col[3 * begin + 1] = temp1;
                        col[3 * begin + 2] = temp2;
                    };
                    change_col(0xe0, 0xe7);    // 0xe0-0xe7
                    change_col(0xf4, 0xfc);    // 0xf4-0xfc
                }
                //cv::Mat image(h, w, CV_8UC4);
                ImageData image(w, h);
                image = 0;
                int p = 0;
                unsigned char* data = (unsigned char*)&grp[idx[m] + 8];
                int datalong = idx[m + 1] - idx[m];
                for (int i = 0; i < h; i++)
                {
                    int yoffset = i * w;
                    unsigned int row = data[p];    // i行数据个数
                    int start = p;
                    p++;
                    if (row > 0)
                    {
                        int x = 0;    // i行目前列
                        for (;;)
                        {
                            x = x + data[p];    // i行空白点个数，跳个透明点
                            if (x >= w)         // i行宽度到头，结束
                            {
                                break;
                            }
                            p++;
                            int solidnum = data[p];    // 不透明点个数
                            p++;
                            for (int j = 0; j < solidnum; j++)
                            {
                                unsigned char index = data[p];
                                //char* pixel = (char*)&(image.at<uint32_t>(yoffset + x));
                                //*(pixel + 0) = 4 * col[3 * index + 2];
                                //*(pixel + 1) = 4 * col[3 * index + 1];
                                //*(pixel + 2) = 4 * col[3 * index + 0];
                                //*(pixel + 3) = 0xff;

                                unsigned char r = 4 * col[3 * data[p] + 0];
                                unsigned char g = 4 * col[3 * data[p] + 1];
                                unsigned char b = 4 * col[3 * data[p] + 2];
                                image.setPixel(x, i, r, g, b, 0xff);

                                p++;
                                x++;
                                if (n == 0)
                                {
                                    //注意一个是8一个是9，取最大值
                                    //若一个图片同时含有这两个范围的颜色，为呈现变化取8，否则严格来说是72
                                    if (index >= 0xe0 && index <= 0xe7)
                                    {
                                        if (max_n == 1 || max_n == 9) { max_n = 8; }
                                    }
                                    if (index >= 0xf4 && index <= 0xfc)
                                    {
                                        if (max_n == 1) { max_n = 9; }
                                    }
                                }
                            }
                            if (x >= w)
                            {
                                break;
                            }    // i行宽度到头，结束
                            if (p - start >= row)
                            {
                                break;
                            }    // i行没有数据，结束
                        }
                        if (p + 1 >= datalong)
                        {
                            break;
                        }
                    }
                }
                std::string image_filename = std::format("{}/{}.png", path, m);
                if (slash && max_n > 1)
                {
                    image_filename = std::format("{}/{}_{}.png", path, m, n);
                }
                //cv::imwrite(image_filename, image);
                image.saveToFile(image_filename);
                if (!slash || n + 1 >= max_n)
                {
                    break;
                }
            }
        }
        filefunc::writeVectorToFile(offset, path + "/index.ka");
        std::print("{} seconds\n", timer.getElapsedTime());
    }

    //std::print("{}", offset);
    return 0;
}

int main(int argc, char* argv[])
{
    if (argc < 5)
    {
        std::print("Usage: {} <idx> <grp> <path_to_save> <slash>\n", argv[0]);
        return 1;
    }
    convert_grpidx_to_png(argv[1], argv[2], argv[3], atoi(argv[4]));
    return 0;
}