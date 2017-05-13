#include "Audio.h"

Audio Audio::m_Audio;


Audio::Audio()
{
    BASS_Init(-1, 22050, BASS_DEVICE_3D, 0, nullptr);
}


Audio::~Audio()
{
    for (auto& i : m_vcMusic)
    {
        BASS_StreamFree(i);
    }
    for (auto& i : m_vcAsound)
    {
        BASS_SampleFree(i);
    }
    for (auto& i : m_vcEsound)
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
		m_vcMusic.push_back(m);
        auto a = BASS_SampleLoad(false, "", 0, 0, 1, flag);
		m_vcAsound.push_back(a);
        auto e = BASS_SampleLoad(false, "", 0, 0, 1, flag);
		m_vcEsound.push_back(e);
    }
}

void Audio::playMusic(int num)
{

}

void Audio::playSound(int num)
{

}

