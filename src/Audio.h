#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#ifdef USE_BASS
#include "bass.h"
#include "bassmidi.h"
using MUSIC = HSTREAM;
using WAV = HSAMPLE;
using MIDI_FONT = BASS_MIDI_FONT;
#else
#include "SDL3_mixer/SDL_mixer.h"
using MUSIC = MIX_Audio*;
using WAV = MIX_Audio*;
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
#ifndef USE_BASS
    MIX_Track* track_music_{};
    std::vector<MIX_Track*> track_wav_;
    MIX_Mixer* mixer_{};
    int current_track_num_ = 0;
#endif

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

    int getCurrentMusic() const { return current_music_index_; }

private:
    MUSIC loadMusic(const std::string& file);
    static WAV loadWav(const std::string& file);
    void playMusic(MUSIC m);
    void playWav(WAV w, int volume, int track_num = -1);
};
