#pragma once
#include <vector>

#ifndef USE_SDL_MIXER_AUDIO
#include "bass.h"
#include "bassmidi.h"
using MUSIC = HSTREAM;
using WAV = HSAMPLE;
using MIDI_FONT = BASS_MIDI_FONT;
#else
#include "SDL2/SDL_mixer.h"
using MUSIC = Mix_Music*;
using WAV = Mix_Chunk*;
using MIDI_FONT = void*;
#endif

class Audio
{
private:
    Audio();
    virtual ~Audio();

    std::vector<MUSIC> music_;
    std::vector<WAV> asound_, esound_;
    MIDI_FONT mid_sound_font_;
    MUSIC current_music_;
    WAV current_sound_;

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
