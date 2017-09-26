#pragma once
#include "bass.h"
#include "bassmidi.h"
#include <vector>

class Audio
{
private:
    Audio();
    virtual ~Audio();

    std::vector<HSTREAM> music_;
    std::vector<HSAMPLE> asound_, esound_;
    BASS_MIDI_FONT sound_font_;

    static Audio audio_;
public:
    static Audio* getInstance() { return &audio_; }

    void init();
    void playMusic(int num);
    void playSound(int num);
};

