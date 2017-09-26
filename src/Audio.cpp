#include "Audio.h"

Audio Audio::audio_;

Audio::Audio()
{
    BASS_Init(-1, 22050, BASS_DEVICE_3D, 0, nullptr);
    //音色库，未完成
}


Audio::~Audio()
{
    for (auto& i : music_)
    {
        BASS_StreamFree(i);
    }
    for (auto& i : asound_)
    {
        BASS_SampleFree(i);
    }
    for (auto& i : esound_)
    {
        BASS_SampleFree(i);
    }
    BASS_Free();
}

//未完成
void Audio::init()
{
    auto flag = BASS_SAMPLE_3D | BASS_SAMPLE_MONO;
    for (int i = 0; i < 100; i++)
    {
        auto m = BASS_StreamCreateFile(false, "", 0, 0, 0);
        music_.push_back(m);
        auto a = BASS_SampleLoad(false, "", 0, 0, 1, flag);
        asound_.push_back(a);
        auto e = BASS_SampleLoad(false, "", 0, 0, 1, flag);
        esound_.push_back(e);
    }
}

void Audio::playMusic(int num)
{

}

void Audio::playSound(int num)
{

}

