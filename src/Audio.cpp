#include "Audio.h"
#include "File.h"
#include "fmt/format.h"

Audio::Audio()
{
#ifndef USE_SDL_MIXER_AUDIO
    if (!BASS_Init(-1, 22050, BASS_DEVICE_3D, 0, nullptr))
    {
        fmt::print("Init Bass fault!\n");
    }
#else
    Mix_Init(MIX_INIT_MP3);
    if (Mix_OpenAudio(22500, MIX_DEFAULT_FORMAT, 2, 4096) == -1)
    {
        fmt::print("Mix_OpenAudio: {}\n", Mix_GetError());
    }
#endif
    init();
}

Audio::~Audio()
{
#ifndef USE_SDL_MIXER_AUDIO
    if (mid_sound_font_.font)
    {
        BASS_MIDI_FontFree(mid_sound_font_.font);
    }
    BASS_Free();
#else
    //如使用SDL_Mixer播放音频，则销毁应该在SDL_Quit之前，此处的单例设计无法保证，故暂时不处理
    /*
    for (auto m : music_)
    {
        Mix_FreeMusic(m);
    }
    for (auto a : asound_)
    {
        Mix_FreeChunk(a);
    }
    for (auto e : esound_)
    {
        Mix_FreeChunk(e);
    }
    Mix_Quit();
    */
#endif
}

void Audio::init()
{
    std::string music_path;
    std::string asound_path;
    std::string esound_path;
#ifndef USE_SDL_MIXER_AUDIO
    auto flag = BASS_SAMPLE_3D | BASS_SAMPLE_MONO;
    mid_sound_font_.font = BASS_MIDI_FontInit("../game/music/mid.sf2", 0);
    mid_sound_font_.preset = -1;
    mid_sound_font_.bank = 0;
    BASS_MIDI_StreamSetFonts(0, &mid_sound_font_, 1);
#endif
    for (int i = 0; i < 100; i++)
    {
        music_path = fmt::format("../game/music/{}.mid", i);
        if (File::fileExist(music_path))
        {
#ifndef USE_SDL_MIXER_AUDIO
            auto m = BASS_MIDI_StreamCreateFile(false, music_path.c_str(), 0, 0, 0, 0);
            BASS_MIDI_StreamSetFonts(m, &mid_sound_font_, 1);
#else
            auto m = Mix_LoadMUS(music_path.c_str());
#endif
            music_.push_back(m);
        }
        else
        {
            music_path = fmt::format("../game/music/{}.mp3", i);
#ifndef USE_SDL_MIXER_AUDIO
            auto m = BASS_StreamCreateFile(false, music_path.c_str(), 0, 0, flag);
#else
            auto m = Mix_LoadMUS(music_path.c_str());
#endif
            music_.push_back(m);
        }
        //int error_t = BASS_ErrorGetCode();

        asound_path = fmt::format("../game/sound/atk{:02}.wav", i);
#ifndef USE_SDL_MIXER_AUDIO
        auto a = BASS_StreamCreateFile(false, asound_path.c_str(), 0, 0, flag);
#else
        auto a = Mix_LoadWAV(asound_path.c_str());
#endif
        asound_.push_back(a);

        esound_path = fmt::format("../game/sound/e{:02}.wav", i);
#ifndef USE_SDL_MIXER_AUDIO
        auto e = BASS_StreamCreateFile(false, esound_path.c_str(), 0, 0, flag);
#else
        auto e = Mix_LoadWAV(esound_path.c_str());
#endif
        esound_.push_back(e);
    }
}

void Audio::playMusic(int num)
{
    if (num < 0 || num >= music_.size() || music_[num] == 0)
    {
        return;
    }
    stopMusic();
#ifndef USE_SDL_MIXER_AUDIO
    BASS_ChannelSetAttribute(music_[num], BASS_ATTRIB_VOL, volume_ / 100.0);
    BASS_Apply3D();
    BASS_ChannelFlags(music_[num], BASS_SAMPLE_LOOP, BASS_SAMPLE_LOOP);
    BASS_ChannelPlay(music_[num], false);
#else
    Mix_VolumeMusic(volume_);
    Mix_FadeInMusic(music_[num], -1, 500);
#endif
    current_music_ = music_[num];
}

void Audio::playASound(int num)
{
    if (num < 0 || num >= asound_.size() || asound_[num] == 0)
    {
        return;
    }
    //BASS_ChannelStop(current_sound_);
#ifndef USE_SDL_MIXER_AUDIO
    auto ch = BASS_SampleGetChannel(asound_[num], false);
    BASS_ChannelSetAttribute(ch, BASS_ATTRIB_VOL, volume_ / 100.0);
    BASS_ChannelPlay(asound_[num], false);
#else
    Mix_Volume(-1, volume_);
    Mix_PlayChannel(-1, asound_[num], 0);
#endif
    current_sound_ = asound_[num];
}

void Audio::playESound(int num)
{
    if (num < 0 || num >= esound_.size() || esound_[num] == 0)
    {
        return;
    }
#ifndef USE_SDL_MIXER_AUDIO
    auto ch = BASS_SampleGetChannel(esound_[num], false);
    BASS_ChannelSetAttribute(ch, BASS_ATTRIB_VOL, volume_ / 100.0);
    BASS_ChannelPlay(esound_[num], false);
#else
    Mix_Volume(-1, volume_);
    Mix_PlayChannel(-1, esound_[num], 0);
#endif
    current_sound_ = esound_[num];
}

void Audio::pauseMusic()
{
#ifndef USE_SDL_MIXER_AUDIO
    BASS_ChannelPause(current_music_);
#else
    Mix_PausedMusic();
#endif
}

void Audio::continueMusic()
{
#ifndef USE_SDL_MIXER_AUDIO
    BASS_ChannelPlay(current_music_, false);
#else
    Mix_ResumeMusic();
#endif
}

void Audio::stopMusic()
{
#ifndef USE_SDL_MIXER_AUDIO
    BASS_ChannelStop(current_music_);
#else
    Mix_HaltMusic();
#endif
}
