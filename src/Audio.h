#pragma once
#include <vector>

#ifndef _SDL_MIXER_AUDIO
#include "bass.h"
#include "bassmidi.h"
typedef HSTREAM MUSIC;
typedef HSAMPLE WAV;
typedef BASS_MIDI_FONT MIDI_FONT;
#else
#include "SDL2/SDL_mixer.h"
typedef Mix_Music* MUSIC;
typedef Mix_Chunk* WAV;
typedef void* MIDI_FONT;
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
