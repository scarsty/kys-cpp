#include "Zip.h"
#include "minizip/zip.h"
#include "minizip/unzip.h"

Zip::Zip()
{
}

Zip::~Zip()
{
}

int Zip::zip(std::string zip_file, std::vector<std::string> files)
{
    zip_fileinfo zi = { 0 };
    zipFile zip = zipOpen(zip_file.c_str(), APPEND_STATUS_CREATE);
    for (auto filename : files)
    {
        FILE* fp;
        if ((fp = fopen(filename.c_str(), "rb")) == NULL)
        {
            break;
        }
        fseek(fp, 0, SEEK_END);
        int length = ftell(fp);
        fseek(fp, 0, 0);
        auto s = (char*)malloc(length + 1);
        fread(s, length, 1, fp);
        fclose(fp);
        //the interface of "file date" is not uniform, to suit it is difficult
        //get_file_date(zip_file.c_str(), &zi.dos_date);
        //zi.dos_date = time(NULL);
        zipOpenNewFileInZip(zip, filename.c_str(), &zi, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_DEFAULT_COMPRESSION);
        zipWriteInFileInZip(zip, s, length);
        zipCloseFileInZip(zip);
        free(s);
    }
    zipClose(zip, NULL);
    return 0;
}

int Zip::unzip(std::string zip_file, std::vector<std::string> files)
{
    unzFile zip;
    zip = unzOpen(zip_file.c_str());
    for (auto filename : files)
    {
        unz_file_info zi = { 0 };
        if (unzLocateFile(zip, filename.c_str(), NULL) != UNZ_OK)
        {
            break;
        }
        char s[100];
        unzGetCurrentFileInfo(zip, &zi, s, 100, NULL, 0, NULL, 0);
        unzOpenCurrentFile(zip);

        FILE* fp;
        if ((fp = fopen(filename.c_str(), "wb")) == NULL)
        {
            break;
        }
        const int size_buf = 8192;
        void* buf = malloc(size_buf);
        uint32_t c = 0;
        while (c < zi.uncompressed_size)
        {
            int len = zi.uncompressed_size - c;
            if (len > size_buf) { len = size_buf; }
            unzReadCurrentFile(zip, buf, size_buf);
            fwrite(buf, len, 1, fp);
            c += size_buf;
        }
        fclose(fp);
        free(buf);
    }
    unzClose(zip);
    return 0;
}