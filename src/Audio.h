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
    BASS_MIDI_FONT mid_sound_font_;
    HSTREAM current_music_;
    HSTREAM current_sound_;

    int volume_ = 50;

public:
    static Audio* getInstance() 
    {
        static Audio a;
        return &a;
    }

    void init();
    void playMusic(int num);
    void playASound(int num);
    void playESound(int num);
    void pauseMusic();
    void continueMusic();
    void stopMusic();

    void setVolume(int v) { volume_ = v; }

};
