#pragma once
#include <string>

bool read_png_offset(const std::string& filename, int& x_offset, int& y_offset);
bool read_png_offset(char* buf, int length, int& x_offset, int& y_offset);

void write_png_offset(const std::string& filename, int x_offset, int y_offset);

void test_png_offset();