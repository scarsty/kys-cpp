#pragma once
#include "bass.h"
#include "bassmidi.h"
#include <vector>

/**
* @file		 Audio.h
* @brief	 √ΩÃÂ¿‡
* @author    bttt

*/
class Audio
{
private:
    Audio();
    virtual ~Audio();

    std::vector<HSTREAM> m_vcMusic;
    std::vector<HSAMPLE> m_vcAsound, m_vcEsound;
    BASS_MIDI_FONT m_sf;

    static Audio m_Audio;
public:
    static Audio* getInstance() { return &m_Audio; }

    void init();
    void playMusic(int num);
    void playSound(int num);
};

