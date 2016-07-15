#include "Audio.h"

Audio Audio::audio;


Audio::Audio()
{
    BASS_Init(-1, 22050, BASS_DEVICE_3D, 0, nullptr);
}


Audio::~Audio()
{
    for (auto& i : music)
    {
        BASS_StreamFree(i);
    }
    for (auto& i : asound)
    {
        BASS_SampleFree(i);
    }
    for (auto& i : esound)
    {
        BASS_SampleFree(i);
    }
    BASS_Free();
}

void Audio::init()
{
    auto flag = BASS_SAMPLE_3D | BASS_SAMPLE_MONO;
    for (int i = 0; i < 100; i++)
    {
        auto m = BASS_StreamCreateFile(false, "", 0, 0, 0);
        music.push_back(m);
        auto a = BASS_SampleLoad(false, "", 0, 0, 1, flag);
        asound.push_back(a);
        auto e = BASS_SampleLoad(false, "", 0, 0, 1, flag);
        esound.push_back(e);
    }
}

void Audio::playMusic(int num)
{

}

void Audio::playSound(int num)
{

}

