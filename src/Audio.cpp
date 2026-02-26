#include "Audio.h"
#include "Engine.h"
#include "GameUtil.h"
#include "filefunc.h"

#ifdef USE_BASS
Audio::Audio()
{
    if (!BASS_Init(-1, 22050, 0, 0, nullptr))
    {
        LOG("Init Bass fault!\n");
    }
    init();
}

Audio::~Audio()
{
    if (mid_sound_font_.font)
    {
        BASS_MIDI_FontFree(mid_sound_font_.font);
    }
    BASS_Stop();
    BASS_Free();
}
#else
Audio::Audio()
{
    MIX_Init();
    SDL_AudioSpec spec;
    spec.freq = 22050;
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;
    mixer_ = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (mixer_ == nullptr)
    {
        LOG("MIX_CreateMixerDevice error: {}\n", SDL_GetError());
    }
    track_music_ = MIX_CreateTrack(mixer_);
    track_wav_.resize(20);
    for (auto& t : track_wav_)
    {
        t = MIX_CreateTrack(mixer_);
    }
    init();
}

Audio::~Audio()
{
    //如使用SDL_Mixer播放音频，则销毁应该在SDL_Quit之前，此处的单例设计无法保证，故暂时不处理
}
#endif

void Audio::init()
{
#ifdef USE_BASS
    mid_sound_font_.font = BASS_MIDI_FontInit((GameUtil::PATH() + "music/mid.sf2").c_str(), 0);
    mid_sound_font_.preset = -1;
    mid_sound_font_.bank = 0;
    BASS_MIDI_StreamSetFonts(0, &mid_sound_font_, 1);
#endif
    // Audio files are now loaded on demand in play*() methods
}

void Audio::playMusic(int num)
{
    if (num < 0)
    {
        return;
    }
    // Lazy load on first access
    if (music_.find(num) == music_.end())
    {
        auto path = std::format("{}music/{}.mid", GameUtil::PATH(), num);
        music_[num] = loadMusic(path);
        if (music_[num] == 0)
        {
            path = std::format("{}music/{}.mp3", GameUtil::PATH(), num);
            music_[num] = loadMusic(path);
        }
    }
    if (music_[num] == 0)
    {
        return;
    }
    stopMusic();
    playMusic(music_[num]);
    current_music_ = music_[num];
    current_music_index_ = num;
}

void Audio::playASound(int num, int volume)
{
    if (num < 0)
    {
        return;
    }
    // Lazy load on first access
    if (asound_.find(num) == asound_.end())
    {
        auto path = std::format("{}sound/atk{:02}.wav", GameUtil::PATH(), num);
        asound_[num] = loadWav(path);
    }
    if (asound_[num] == 0)
    {
        return;
    }
    if (volume < 0 || volume > 100)
    {
        volume = volume_wav_;
    }
    playWav(asound_[num], volume);
    current_sound_ = asound_[num];
}

void Audio::playESound(int num, int volume)
{
    if (num < 0)
    {
        return;
    }
    // Lazy load on first access
    if (esound_.find(num) == esound_.end())
    {
        auto path = std::format("{}sound/e{:02}.wav", GameUtil::PATH(), num);
        esound_[num] = loadWav(path);
    }
    if (esound_[num] == 0)
    {
        return;
    }
    if (volume < 0 || volume > 100)
    {
        volume = volume_wav_;
    }
    playWav(esound_[num], volume);
    current_sound_ = esound_[num];
}

void Audio::pauseMusic()
{
#ifdef USE_BASS
    BASS_ChannelPause(current_music_);
#else
    MIX_PauseTrack(track_music_);
#endif
}

void Audio::resumeMusic()
{
#ifdef USE_BASS
    BASS_ChannelPlay(current_music_, false);
#else
    MIX_ResumeTrack(track_music_);
#endif
}

void Audio::stopMusic()
{
#ifdef USE_BASS
    BASS_ChannelStop(current_music_);
#else
    MIX_StopTrack(track_music_, 22050);  // ~1 second fade at 22050 Hz (param is sample frames, not ms)
#endif
}

void Audio::stopWav()
{
#ifdef USE_BASS
    BASS_ChannelStop(current_sound_);
#else
    MIX_StopTrack(track_wav_[0], 0);
#endif
}

void Audio::playVoice(int voice_id, int volume)
{
    stopWav();
    if (voice_.find(voice_id) == voice_.end())
    {
        auto filename = std::format("{}voice/{}.mp3", GameUtil::PATH(), voice_id);
        if (!filefunc::fileExist(filename))
        {
            filename = std::format("{}voice/{}.wav", GameUtil::PATH(), voice_id);
        }
        voice_[voice_id] = loadWav(filename);
    }
    playWav(voice_[voice_id], volume, 0);
    current_sound_ = voice_[voice_id];
}

MUSIC Audio::loadMusic(const std::string& file)
{
#ifdef USE_BASS
    MUSIC m{};
    if (file.contains(".mid"))
    {
        m = BASS_MIDI_StreamCreateFile(false, file.c_str(), 0, 0, 0, 0);
        BASS_MIDI_StreamSetFonts(m, &mid_sound_font_, 1);
    }
    else
    {
        m = BASS_StreamCreateFile(false, file.c_str(), 0, 0, 0);
    }
    return m;
#else
    if (file.contains(".mid"))
    {
        // Load MIDI with fluidsynth + soundfont
        Prop pro;
        auto io = SDL_IOFromFile(file.c_str(), "rb");
        if (!io)
        {
            LOG("loadMusic: failed to open {}: {}\n", file, SDL_GetError());
            return nullptr;
        }
        pro.set(MIX_PROP_AUDIO_LOAD_IOSTREAM_POINTER, io);
        pro.set(MIX_PROP_AUDIO_DECODER_STRING, "fluidsynth");
        pro.set("SDL_mixer.decoder.fluidsynth.soundfont_path", (GameUtil::PATH() + "music/mid.sf2").c_str());
        auto m = MIX_LoadAudioWithProperties(pro.id());
        SDL_CloseIO(io);
        if (!m)
        {
            LOG("loadMusic: fluidsynth failed for {}: {}\n", file, SDL_GetError());
        }
        return m;
    }
    auto m = MIX_LoadAudio(nullptr, file.c_str(), false);
    if (!m)
    {
        LOG("loadMusic: failed to load {}: {}\n", file, SDL_GetError());
    }
    return m;
#endif
}

WAV Audio::loadWav(const std::string& file)
{
#ifdef USE_BASS
    auto s = BASS_StreamCreateFile(false, file.c_str(), 0, 0, 0);
    if (s == 0)
    {
        s = BASS_SampleLoad(false, file.c_str(), 0, 0, 1, 0);
    }
    return s;
#else
    return MIX_LoadAudio(nullptr, file.c_str(), false);
#endif
}

void Audio::playMusic(MUSIC m)
{
#ifdef USE_BASS
    BASS_CHANNELINFO info;
    if (BASS_ChannelGetInfo(m, &info))
    {
        // Check if this is a MIDI stream and set MIDI-specific volume
        if (info.ctype == BASS_CTYPE_STREAM_MIDI)
        {
            LOG("This is a MIDI stream, setting MIDI volume attribute\n");
            BASS_ChannelSetAttribute(m, BASS_ATTRIB_MIDI_VOL, volume_ / 100.0);
        }
    }

    BASS_ChannelSetAttribute(m, BASS_ATTRIB_VOL, volume_ / 100.0);
    BASS_ChannelFlags(m, BASS_SAMPLE_LOOP, BASS_SAMPLE_LOOP);
    BASS_ChannelPlay(m, false);
#else
    LOG("playMusic: m={}, volume={}, track={}\n", (void*)m, volume_, (void*)track_music_);
    MIX_SetTrackGain(track_music_, volume_ / 100.0f);
    MIX_SetTrackAudio(track_music_, m);
    Prop pro;
    pro.set(MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER, 500);
    pro.set(MIX_PROP_PLAY_LOOPS_NUMBER, -1);
    bool ok = MIX_PlayTrack(track_music_, pro.id());
    if (!ok)
    {
        LOG("playMusic: MIX_PlayTrack failed: {}\n", SDL_GetError());
    }
#endif
}

void Audio::playWav(WAV w, int volume, int track_num)
{
    if (volume < 0 || volume > 100)
    {
        volume = volume_wav_;
    }
#ifdef USE_BASS
    auto ch = BASS_SampleGetChannel(w, 1);
    if (ch == 0)
    {
        // w is already a stream, not a sample
        ch = w;
    }
    BASS_ChannelSetAttribute(ch, BASS_ATTRIB_VOL, volume / 100.0);
    BASS_ChannelPlay(ch, false);
#else
    int tnum = track_num;
    if (tnum < 0)
    {
        tnum = current_track_num_;
    }
    MIX_SetTrackGain(track_wav_[tnum], volume / 100.0f);
    MIX_SetTrackAudio(track_wav_[tnum], w);
    MIX_PlayTrack(track_wav_[tnum], 0);
    current_track_num_++;
    if (current_track_num_ >= track_wav_.size())
    {
        current_track_num_ = 0;
    }
#endif
}
