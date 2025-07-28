#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#ifndef USE_SDL_MIXER_AUDIO
#include "bass.h"
#include "bassmidi.h"
using MUSIC = HSTREAM;
using WAV = HSAMPLE;
using MIDI_FONT = BASS_MIDI_FONT;
#else
#include "SDL3_mixer/SDL_mixer.h"
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
    std::unordered_map<int, WAV> voice_;
    MIDI_FONT mid_sound_font_;
    MUSIC current_music_;
    WAV current_sound_;

    int volume_ = 20;
    int volume_wav_ = 50;
    int current_music_index_ = -1;

public:
    static Audio* getInstance()
    {
        static Audio a;
        return &a;
    }

    void init();
    void playMusic(int num);
    void playASound(int num, int volume = -1);
    void playESound(int num, int volume = -1);
    void pauseMusic();
    void resumeMusic();
    void stopMusic();

    void stopWav();

    void setVolume(int v) { volume_ = v; }
    void setVolumeWav(int v) { volume_wav_ = v; }

    void playVoice(int voice_id, int volume = -1);

    MUSIC loadMusic(const std::string& file);
    static WAV loadWav(const std::string& file);

    void playMUSIC(MUSIC m);
    void playWAV(WAV w, int volume);

    int getCurrentMusic() const { return current_music_index_; }
};
