#include "Audio.h"
#include "others/libconvert.h"
#include "File.h"

Audio Audio::audio_;

Audio::Audio()
{
    if (!BASS_Init(-1, 22050, BASS_DEVICE_3D, 0, nullptr))
    {
        printf("Init Bass fault!\n");
    }
    init();
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

//Î´Íê³É
void Audio::init()
{
    auto flag = BASS_SAMPLE_3D | BASS_SAMPLE_MONO;
    std::string music_path;
    std::string asound_path;
    std::string esound_path;
    BASS_MIDI_FONT sf;
    sf.font = BASS_MIDI_FontInit("../game/music/mid.sf2", 0);
    sf.preset = -1;
    sf.bank = 0;
    BASS_MIDI_StreamSetFonts(0, &sf, 1);
    for (int i = 0; i < 100; i++)
    {
        music_path = convert::formatString("../game/music/%d.mid", i);
        if (File::fileExist(music_path))
        {
            auto m = BASS_MIDI_StreamCreateFile(false, music_path.c_str(), 0, 0, 0, 0);
            BASS_MIDI_StreamSetFonts(m, &sf, 1);
            music_.push_back(m);
        }
        else
        {
            music_path = convert::formatString("../game/music/%d.mp3", i);
            auto m = BASS_StreamCreateFile(false, music_path.c_str(), 0, 0, flag);
            music_.push_back(m);
        }
        //int error_t = BASS_ErrorGetCode();

        asound_path = convert::formatString("../game/sound/atk%02d.wav", i);
        auto a = BASS_StreamCreateFile(false, asound_path.c_str(), 0, 0, flag);
        asound_.push_back(a);

        esound_path = convert::formatString("../game/sound/e%02d.wav", i);
        auto e = BASS_StreamCreateFile(false, esound_path.c_str(), 0, 0, flag);
        esound_.push_back(e);
    }
}

void Audio::playMusic(int num)
{
    if (num < 0) { return; }
    BASS_ChannelStop(current_music_);
    BASS_ChannelPlay(music_[num], true);
    //int error_t = BASS_ErrorGetCode();
    current_music_ = music_[num];
}

void Audio::playASound(int num)
{
    if (num < 0) { return; }
    //BASS_ChannelStop(current_sound_);
    BASS_ChannelPlay(asound_[num], true);
    current_sound_ = asound_[num];
}

void Audio::playESound(int num)
{
    if (num < 0) { return; }
    //BASS_ChannelStop(current_sound_);
    BASS_ChannelPlay(esound_[num], true);
    current_sound_ = esound_[num];
}

void Audio::PauseMusic()
{
    BASS_ChannelPause(current_music_);
}

void Audio::ContinueMusic()
{
    BASS_ChannelPlay(current_music_, false);
}

void Audio::StopMusic()
{
    BASS_ChannelStop(current_music_);
}


