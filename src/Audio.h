#pragma once
#include "bass.h"
#include "bassmidi.h"
#include <vector>

class Audio
{
private:
    Audio();
    virtual ~Audio();

    std::vector<HSTREAM> music;
    std::vector<HSAMPLE> asound, esound;
    BASS_MIDI_FONT sf;

    static Audio audio;
public:
    static Audio* getInstance() { return &audio; }

    void init();
    void playMusic(int num);
    void playSound(int num);
};

