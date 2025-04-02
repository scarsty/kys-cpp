#pragma once
#include "Engine.h"
#include <string>

class Video
{
private:
    void* smallpot_ = nullptr;

public:
    Video(Window* w);
    ~Video();
    int playVideo(const std::string& filename);
};
