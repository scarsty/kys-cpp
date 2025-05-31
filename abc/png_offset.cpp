
#include "png_offset.h"
#include "png.h"

#include <cstring>
#include <stdexcept>
#include <vector>

class PngClass
{
    png_structp png_r{}, png_w{};
    png_infop info{};
    std::vector<png_bytep> raw;
    struct png_io_ptr
    {
        png_bytep buffer_ptr;
        png_size_t buffer_length;
        png_size_t readed_length = 0;
    };
    png_io_ptr io_ptr;

    FILE* fp{};
    std::string filename;

public:
    PngClass(int for_write = 0)
    {
        png_r = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png_r)
        {
            throw std::runtime_error("Failed to create PNG read struct");
        }
        info = png_create_info_struct(png_r);
        if (!info)
        {
            png_destroy_read_struct(&png_r, NULL, NULL);
            throw std::runtime_error("Failed to create PNG info struct");
        }
        setjmp(png_jmpbuf(png_r));
    }

    ~PngClass()
    {
        if (fp)
        {
            fclose(fp);
            fp = nullptr;
        }
        for (auto& row : raw)
        {
            if (row)
            {
                free(row);
            }
        }
        raw.clear();
        if (png_r)
        {
            png_destroy_read_struct(&png_r, &info, NULL);
        }
        if (png_w)
        {
            png_destroy_write_struct(&png_w, NULL);
        }
    }

    void init_file(const std::string& filename)
    {
        fp = fopen(filename.c_str(), "rb");
        if (!fp)
        {
            throw std::runtime_error("Failed to open file: " + filename);
        }
        png_init_io(png_r, fp);
    }

    void read()
    {
        if (!raw.empty())
        {
            return;
        }
        png_read_info(png_r, info);
        int width = png_get_image_width(png_r, info);
        int height = png_get_image_height(png_r, info);
        raw.resize(height);
        for (int i = 0; i < height; ++i)
        {
            raw[i] = (png_bytep)malloc(png_get_rowbytes(png_r, info));
        }
        png_read_image(png_r, raw.data());
    }

    void init_buffer(char* buffer, int length)
    {
        if (!buffer || length <= 0)
        {
            throw std::runtime_error("Buffer is null or length is invalid");
        }
        io_ptr = { (png_bytep)buffer, (png_size_t)length };
        png_set_read_fn(png_r, &io_ptr, [](png_structp png_r, png_bytep data, png_size_t length)
            {
                auto p = (png_io_ptr*)png_get_io_ptr(png_r);
                memcpy(data, p->buffer_ptr + p->readed_length, length);
                p->readed_length += length;
                if (p->readed_length > p->buffer_length)
                {
                    png_error(png_r, "Read error: buffer overflow");
                }
            });
    }

    bool read_offset(int& x_offset, int& y_offset)
    {
        png_read_info(png_r, info);
        if (png_get_valid(png_r, info, PNG_INFO_oFFs))
        {
            int unit_type;
            png_get_oFFs(png_r, info, &x_offset, &y_offset, &unit_type);
            return true;
        }
        return false;
    }

    void write_offset(int x_offset, int y_offset)
    {
        png_set_oFFs(png_r, info, x_offset, y_offset, PNG_OFFSET_PIXEL);
    }

    void save_to_file(const std::string& filename)
    {
        read();
        if (filename == this->filename)
        {
            fclose(fp);
            fp = nullptr;
        }
        FILE* fp1 = fopen(filename.c_str(), "wb");
        if (!fp1)
        {
            throw std::runtime_error("Failed to open file for writing: " + filename);
        }
        png_w = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png_w)
        {
            fclose(fp1);
            throw std::runtime_error("Failed to create PNG write struct");
        }
        png_init_io(png_w, fp1);
        png_write_info(png_w, info);
        png_write_image(png_w, raw.data());
        png_write_end(png_w, info);
        fclose(fp1);
    }
};

bool read_png_offset(const std::string& filename, int& x_offset, int& y_offset)
{
    PngClass pc;
    pc.init_file(filename);
    auto ret = pc.read_offset(x_offset, y_offset);
    return ret;
}

bool read_png_offset(char* buf, int length, int& x_offset, int& y_offset)
{
    PngClass pc;
    if (!buf || length <= 0)
    {
        return false;
    }
    pc.init_buffer(buf, length);
    return pc.read_offset(x_offset, y_offset);
}

void write_png_offset(const std::string& filename, int x_offset, int y_offset)
{
    PngClass pc;
    pc.init_file(filename);
    pc.write_offset(x_offset, y_offset);
    pc.save_to_file(filename);
}
