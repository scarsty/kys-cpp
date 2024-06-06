#include "Engine.h"
#ifdef _MSC_VER
#define NOMINMAX
#include <windows.h>
#pragma comment(lib, "user32.lib")
#endif

#if defined(_WIN32) && defined(WITH_SMALLPOT)
#include "PotDll.h"
#endif

Engine::Engine()
{
}

Engine::~Engine()
{
    destroy();
}

int Engine::init(void* handle /*= nullptr*/, int handle_type /*= 0*/, int maximized)
{
    if (inited_)
    {
        return 0;
    }
    inited_ = true;
#ifndef _WINDLL
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC))
    {
        return -1;
    }
#endif
    window_mode_ = handle_type;
    if (handle)
    {
        if (handle_type == 0)
        {
            window_ = SDL_CreateWindowFrom(handle);
        }
        else
        {
            window_ = (BP_Window*)handle;
        }
    }
    else
    {
        uint32_t flags = SDL_WINDOW_RESIZABLE;
        if (maximized)
        {
            flags |= SDL_WINDOW_MAXIMIZED;
        }
        window_ = SDL_CreateWindow(title_.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, start_w_, start_h_, flags);
    }
    //SDL_CreateWindowFrom()
#ifndef _WINDLL
    SDL_ShowWindow(window_);
    SDL_RaiseWindow(window_);
#endif
    renderer_ = SDL_GetRenderer(window_);
    fmt1::print("{}\n", SDL_GetError());
    if (renderer_ == nullptr)
    {
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE /*| SDL_RENDERER_PRESENTVSYNC*/);
        renderer_self_ = true;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    //屏蔽触摸板
    SDL_EventState(SDL_FINGERUP, SDL_DISABLE);
    SDL_EventState(SDL_FINGERDOWN, SDL_DISABLE);
    SDL_EventState(SDL_FINGERMOTION, SDL_DISABLE);

    //手柄
    if (SDL_NumJoysticks() < 1)
    {
        fmt1::print("Warning: No joysticks connected!\n");
    }
    else
    {
        //按照游戏控制器打开
        game_controller_ = SDL_GameControllerOpen(0);
        if (game_controller_)
        {
            fmt1::print("Found {} game controller(s)\n", SDL_NumJoysticks());
            std::string name = SDL_GameControllerName(game_controller_);
            fmt1::print("{}\n", name);
            if (name.find("Switch") != std::string::npos) { switch_ = 1; }
        }
        else
        {
            fmt1::print("Warning: Unable to open game controller! SDL Error: {}\n", SDL_GetError());
        }
    }

    rect_ = { 0, 0, start_w_, start_h_ };
    logo_ = loadImage("logo.png");
    showLogo();
    renderPresent();
    TTF_Init();

#ifdef _MSC_VER
    RECT r;
    SystemParametersInfo(SPI_GETWORKAREA, 0, (PVOID)&r, 0);
    int w = GetSystemMetrics(SM_CXEDGE);
    int h = GetSystemMetrics(SM_CYEDGE);
    min_x_ = r.left + w;
    min_y_ = r.top + h + GetSystemMetrics(SM_CYCAPTION);
    max_x_ = r.right - w;
    max_y_ = r.bottom - h;
#else
    BP_Rect r;
    SDL_GetDisplayBounds(0, &r);
    min_x_ = r.x;
    min_y_ = r.y;
    max_x_ = r.w + r.x;
    max_y_ = r.h + r.y;
#endif

    square_ = createRectTexture(100, 100, 0);

    fmt1::print("maximum width and height are: {}, {}\n", max_x_, max_y_);
#if defined(_WIN32) && defined(WITH_SMALLPOT) && !defined(_DEBUG)
    tinypot_ = PotCreateFromWindow(window_);
#endif
    createMainTexture(-1, BP_TEXTUREACCESS_TARGET, start_w_, start_h_);
    return 0;
}

int Engine::getWindowWidth() const
{
    int w, h;
    getWindowSize(w, h);
    return w;
}

int Engine::getWindowHeight() const
{
    int w, h;
    getWindowSize(w, h);
    return h;
}

void Engine::setWindowIsMaximized(bool b) const
{
    if (b)
    {
        SDL_MaximizeWindow(window_);
    }
    else
    {
        SDL_RestoreWindow(window_);
    }
}

void Engine::setWindowSize(int w, int h)
{
    if (getWindowIsMaximized())
    {
        return;
    }
    if (rotation_ == 90 || rotation_ == 270)
    {
        std::swap(w, h);
    }
    if (w <= 0 || h <= 0)
    {
        return;
    }
    //w = 1920;
    //h = 1080;
    win_w_ = std::min(max_x_ - min_x_, w);
    win_h_ = std::min(max_y_ - min_y_, h);
    double ratio;
    ratio = std::min(1.0 * win_w_ / w, 1.0 * win_h_ / h);
    win_w_ = w * ratio;
    win_h_ = h * ratio;
    //fmt1::print("{}, {}, {}, {}, {}\n", win_w_, win_h_, w, h, ratio);
    if (!window_)
    {
        return;
    }

    SDL_SetWindowSize(window_, win_w_, win_h_);
    setPresentPosition(tex_);

    SDL_ShowWindow(window_);
    SDL_RaiseWindow(window_);
    SDL_GetWindowSize(window_, &win_w_, &win_h_);
    //fmt1::print("{}, {}, {}, {}, {}\n", win_w_, win_h_, w, h, ratio);
    //resetWindowsPosition();
    //renderPresent();
}

void Engine::setWindowPosition(int x, int y) const
{
    int w, h;
    getWindowSize(w, h);
    if (x == BP_WINDOWPOS_CENTERED)
    {
        x = min_x_ + (max_x_ - min_x_ - w) / 2;
    }
    if (y == BP_WINDOWPOS_CENTERED)
    {
        y = min_y_ + (max_y_ - min_y_ - h) / 2;
    }
    SDL_SetWindowPosition(window_, x, y);
}

void Engine::createMainTexture(int pixfmt, BP_TextureAccess a, int w, int h)
{
    if (tex_)
    {
        SDL_DestroyTexture(tex_);
    }
    if (pixfmt < 0)
    {
        tex_ = createARGBRenderedTexture(w, h);
    }
    else
    {
        tex_ = createTexture(pixfmt, a, w, h);
    }
    setPresentPosition(tex_);
}

void Engine::resizeMainTexture(int w, int h) const
{
    int w0, h0;
    uint32_t pix_fmt;
    if (!SDL_QueryTexture(tex_, &pix_fmt, nullptr, &w0, &h0))
    {
        if (w0 != w || h0 != h)
        {
            //createMainTexture(pix_fmt, w, h);
        }
    }
}

//创建一个专用于画场景的，后期放大
void Engine::createAssistTexture(int w, int h)
{
    //tex_ = createYUVTexture(w, h);
    uint32_t pixfmt;
    int a;
    SDL_QueryTexture(tex_, &pixfmt, &a, nullptr, nullptr);
    tex2_ = createTexture(pixfmt, BP_TEXTUREACCESS_TARGET, w, h);
    //tex_ = createARGBRenderedTexture(768, 480);
    //SDL_SetTextureBlendMode(tex2_, SDL_BLENDMODE_BLEND);
}

void Engine::setPresentPosition(BP_Texture* tex)
{
    if (!tex)
    {
        return;
    }
    int w_dst = 0, h_dst = 0;
    int w_src = 0, h_src = 0;
    getWindowSize(w_dst, h_dst);
    SDL_QueryTexture(tex, nullptr, nullptr, &w_src, &h_src);
    w_src *= ratio_x_;
    h_src *= ratio_y_;
    if (keep_ratio_)
    {
        if (w_src == 0 || h_src == 0)
        {
            return;
        }
        double ratio = std::min(1.0 * w_dst / w_src, 1.0 * h_dst / h_src);
        if (rotation_ == 90 || rotation_ == 270)
        {
            ratio = std::min(1.0 * w_dst / h_src, 1.0 * h_dst / w_src);
        }
        rect_.x = (w_dst - w_src * ratio) / 2;
        rect_.y = (h_dst - h_src * ratio) / 2;
        rect_.w = w_src * ratio;
        rect_.h = h_src * ratio;
    }
    else
    {
        //unfinshed
        rect_.x = 0;
        rect_.y = 0;
        rect_.w = w_dst;
        rect_.h = h_dst;
        if (rotation_ == 90 || rotation_ == 270)
        {
            rect_.x = (h_dst - w_dst) / 2;
            rect_.y = (w_dst - h_dst) / 2;
            rect_.w = h_dst;
            rect_.h = w_dst;
        }
    }
}

BP_Texture* Engine::createTexture(uint32_t pix_fmt, BP_TextureAccess a, int w, int h) const
{
    if (pix_fmt == SDL_PIXELFORMAT_UNKNOWN)
    {
        pix_fmt = SDL_PIXELFORMAT_RGB24;
    }
    return SDL_CreateTexture(renderer_, pix_fmt, a, w, h);
}

BP_Texture* Engine::createYUVTexture(int w, int h) const
{
    return SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, w, h);
}

void Engine::updateYUVTexture(BP_Texture* t, uint8_t* data0, int size0, uint8_t* data1, int size1, uint8_t* data2, int size2)
{
    SDL_UpdateYUVTexture(t, nullptr, data0, size0, data1, size1, data2, size2);
}

BP_Texture* Engine::createARGBTexture(int w, int h)
{
    return SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
}

BP_Texture* Engine::createARGBRenderedTexture(int w, int h)
{
    return SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
}

void Engine::updateARGBTexture(BP_Texture* t, uint8_t* buffer, int pitch)
{
    SDL_UpdateTexture(t, nullptr, buffer, pitch);
}

int Engine::lockTexture(BP_Texture* t, BP_Rect* r, void** pixel, int* pitch)
{
    return SDL_LockTexture(t, r, pixel, pitch);
}

void Engine::unlockTexture(BP_Texture* t)
{
    SDL_UnlockTexture(t);
}

void Engine::renderPresent() const
{
    //renderMainTextureToWindow();
    SDL_RenderPresent(renderer_);
    SDL_RenderClear(renderer_);
    //setRenderMainTexture();
}

void Engine::renderCopy(BP_Texture* t /*= nullptr*/, double angle)
{
    SDL_RenderCopyEx(renderer_, t, nullptr, &rect_, angle, nullptr, SDL_FLIP_NONE);
    render_times_++;
}

void Engine::renderCopy(BP_Texture* t, int x, int y, int w, int h, double angle, int inPresent)
{
    if (inPresent == 1)
    {
        x += rect_.x;
        y += rect_.y;
    }
    BP_Rect r = { x, y, w, h };
    renderCopy(t, nullptr, &r, angle);
}

void Engine::renderCopy(BP_Texture* t, BP_Rect* rect0, BP_Rect* rect1, double angle, int inPresent /*= 0*/)
{
    SDL_RenderCopyEx(renderer_, t, rect0, rect1, angle, nullptr, SDL_FLIP_NONE);
    render_times_++;
}

void Engine::destroy() const
{
    destroyTexture(tex_);
    destroyAssistTexture();
    if (renderer_self_)
    {
        SDL_DestroyRenderer(renderer_);
    }
    if (window_mode_ == 0)
    {
        SDL_DestroyWindow(window_);
    }

#ifndef _WINDLL
    SDL_Quit();
#endif
#if defined(_WIN32) && defined(WITH_SMALLPOT) && !defined(_DEBUG)
    PotDestory(tinypot_);
#endif
}

bool Engine::isFullScreen()
{
    Uint32 state = SDL_GetWindowFlags(window_);
    full_screen_ = (state & SDL_WINDOW_FULLSCREEN) || (state & SDL_WINDOW_FULLSCREEN_DESKTOP);
    return full_screen_;
}

void Engine::toggleFullscreen()
{
    full_screen_ = !full_screen_;
    if (full_screen_)
    {
        SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
    else
    {
        SDL_SetWindowFullscreen(window_, 0);
    }
    renderClear();
}

BP_Texture* Engine::loadImage(const std::string& filename, int as_white)
{
    //fmt1::print("%s", filename.c_str());
    auto sur = IMG_Load(filename.c_str());
    if (as_white) { toWhite(sur); }
    auto tex = SDL_CreateTextureFromSurface(renderer_, sur);
    SDL_FreeSurface(sur);
    return tex;
}

BP_Texture* Engine::loadImageFromMemory(const std::string& content, int as_white) const
{
    auto rw = SDL_RWFromConstMem(content.data(), content.size());
    auto sur = IMG_LoadTyped_RW(rw, 1, "png");
    if (as_white) { toWhite(sur); }
    auto tex = SDL_CreateTextureFromSurface(renderer_, sur);
    SDL_FreeSurface(sur);
    return tex;
}

void Engine::toWhite(BP_Surface* sur)
{
    for (int i = 0; i < sur->w * sur->h; i++)
    {
        auto p = (uint32_t*)sur->pixels + i;
        uint8_t r, g, b, a;
        SDL_GetRGBA(*p, sur->format, &r, &g, &b, &a);
        if (a == 0)
        {
            *p = SDL_MapRGBA(sur->format, 255, 255, 255, 0);
        }
        else
        {
            *p = SDL_MapRGBA(sur->format, 255, 255, 255, 255);
        }
    }
}

bool Engine::setKeepRatio(bool b)
{
    return keep_ratio_ = b;
}

BP_Texture* Engine::transBitmapToTexture(const uint8_t* src, uint32_t color, int w, int h, int stride) const
{
    auto s = SDL_CreateRGBSurface(0, w, h, 32, 0xff000000, 0xff0000, 0xff00, 0xff);
    SDL_FillRect(s, nullptr, color);
    auto p = (uint8_t*)s->pixels;
    for (int x = 0; x < w; x++)
    {
        for (int y = 0; y < h; y++)
        {
            p[4 * (y * w + x)] = src[y * stride + x];
        }
    }
    auto t = SDL_CreateTextureFromSurface(renderer_, s);
    SDL_FreeSurface(s);
    setTextureBlendMode(t);
    setTextureAlphaMod(t, 192);
    return t;
}

void Engine::resetWindowPosition()
{
    int x, y, w, h, x0, y0;
    getWindowSize(w, h);
    SDL_GetWindowPosition(window_, &x0, &y0);
    x = std::max(min_x_, x0);
    y = std::max(min_y_, y0);
    if (x + w > max_x_)
    {
        x = std::min(x, max_x_ - w);
    }
    if (y + h > max_y_)
    {
        y = std::min(y, max_y_ - h);
    }
    if (x != x0 || y != y0)
    {
        setWindowPosition(x, y);
    }
}

void Engine::setColor(BP_Texture* tex, BP_Color c)
{
    SDL_SetTextureColorMod(tex, c.r, c.g, c.b);
    setTextureAlphaMod(tex, c.a);
    setTextureBlendMode(tex);
}

void Engine::fillColor(BP_Color color, int x, int y, int w, int h) const
{
    if (w < 0 || h < 0)
    {
        getWindowSize(w, h);
    }
    BP_Rect r{ x, y, w, h };
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_RenderFillRect(renderer_, &r);
}

void Engine::renderMainTextureToWindow()
{
    resetRenderTarget();
    renderCopy(tex_, nullptr, nullptr);
}

void Engine::renderAssistTextureToMain()
{
    setRenderTarget(tex_);
    renderCopy(tex2_, nullptr, nullptr);
}

void Engine::mixAudio(Uint8* dst, const Uint8* src, Uint32 len, int volume) const
{
    SDL_MixAudioFormat(dst, src, audio_format_, len, volume);
}

int Engine::openAudio(int& freq, int& channels, int& size, int minsize, AudioCallback f)
{
    SDL_AudioSpec want;
    SDL_zero(want);

    fmt1::print("\naudio freq/channels: stream {}/{}, ", freq, channels);
    if (channels <= 2)
    {
        channels = 2;
    }
    want.freq = freq;
    want.format = AUDIO_S16;
    want.channels = channels;
    want.samples = size;
    want.callback = mixAudioCallback;
    //want.userdata = this;
    want.silence = 0;

    audio_callback_ = f;
    //if (useMap())
    {
        want.samples = std::max(size, minsize);
    }

    audio_device_ = 0;
    int i = 10;
    while (audio_device_ == 0 && i > 0)
    {
        audio_device_ = SDL_OpenAudioDevice(NULL, 0, &want, &audio_spec_, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
        want.channels--;
        i--;
    }
    fmt1::print("device {}/{}\n", audio_spec_.freq, audio_spec_.channels);

    audio_format_ = audio_spec_.format;

    if (audio_device_)
    {
        SDL_PauseAudioDevice(audio_device_, 0);
    }
    else
    {
        fmt1::print("failed to open audio: {}\n", SDL_GetError());
    }

    freq = audio_spec_.freq;
    channels = audio_spec_.channels;

    return 0;
}

void Engine::mixAudioCallback(void* userdata, Uint8* stream, int len)
{
    SDL_memset(stream, 0, len);
    if (getInstance()->audio_callback_)
    {
        getInstance()->audio_callback_(stream, len);
    }
}

void Engine::getMouseState(int& x, int& y)
{
    SDL_GetMouseState(&x, &y);
}

void Engine::getMouseStateInStartWindow(int& x, int& y) const
{
    SDL_GetMouseState(&x, &y);
    int w, h;
    getWindowSize(w, h);
    x = x * start_w_ / w;
    y = y * start_h_ / h;
}

void Engine::setMouseState(int x, int y) const
{
    SDL_WarpMouseInWindow(window_, x, y);
}

void Engine::setMouseStateInStartWindow(int x, int y) const
{
    int w, h;
    getWindowSize(w, h);
    x = x * w / start_w_;
    y = y * h / start_h_;
    SDL_WarpMouseInWindow(window_, x, y);
}

int Engine::pollEvent(BP_Event& e) const
{
    int r = SDL_PollEvent(&e);
    if (switch_)
    {
        if (e.type == BP_CONTROLLERBUTTONDOWN || e.type == BP_CONTROLLERBUTTONUP)
        {
            auto& key = e.cbutton.button;
            if (key == BP_CONTROLLER_BUTTON_A) { key = BP_CONTROLLER_BUTTON_B; }
            else if (key == BP_CONTROLLER_BUTTON_B) { key = BP_CONTROLLER_BUTTON_A; }
            else if (key == BP_CONTROLLER_BUTTON_X) { key = BP_CONTROLLER_BUTTON_Y; }
            else if (key == BP_CONTROLLER_BUTTON_Y) { key = BP_CONTROLLER_BUTTON_X; }
        }
    }
    return r;
}

bool Engine::checkKeyPress(BP_Keycode key)
{
    return SDL_GetKeyboardState(NULL)[SDL_GetScancodeFromKey(key)];
}

bool Engine::gameControllerGetButton(int key) const
{
    if (game_controller_)
    {
        if (switch_)
        {
            if (key == BP_CONTROLLER_BUTTON_A) { key = BP_CONTROLLER_BUTTON_B; }
            else if (key == BP_CONTROLLER_BUTTON_B) { key = BP_CONTROLLER_BUTTON_A; }
            else if (key == BP_CONTROLLER_BUTTON_X) { key = BP_CONTROLLER_BUTTON_Y; }
            else if (key == BP_CONTROLLER_BUTTON_Y) { key = BP_CONTROLLER_BUTTON_X; }
        }
        return SDL_GameControllerGetButton(game_controller_, SDL_GameControllerButton(key));
    }
    return false;
}

int16_t Engine::gameControllerGetAxis(int axis) const
{
    if (game_controller_)
    {
        return SDL_GameControllerGetAxis(game_controller_, SDL_GameControllerAxis(axis));
    }
    return 0;
}

void Engine::gameControllerRumble(int l, int h, uint32_t time) const
{
    if (game_controller_)
    {
        auto s = SDL_GameControllerRumble(game_controller_, l * 65535 / 100, h * 65535 / 100, time);
    }
}

BP_Texture* Engine::createRectTexture(int w, int h, int style) const
{
    auto square_s = SDL_CreateRGBSurface(0, w, h, 32, RMASK, GMASK, BMASK, AMASK);

    //SDL_FillRect(square_s, nullptr, 0xffffffff);
    BP_Rect r = { 0, 0, 1, 1 };
    auto& x = r.x;
    auto& y = r.y;
    uint8_t a = 0;
    for (x = 0; x < w; x++)
    {
        for (y = 0; y < h; y++)
        {
            int c;
            if (style == 0)
            {
                a = 100 + 150 * cos(M_PI * (1.0 * y / w - 0.5));
                c = 0x00ffffff | (a << 24);
            }
            else if (style == 1)
            {
                c = 0xffffffff;
            }
            SDL_FillRect(square_s, &r, c);
            /*if ((x - d / 2)*(x - d / 2) + (y - d / 2)*(y - d / 2) < (d / 2) * (d / 2))
            {
                SDL_FillRect(square_s, &r, 0x00ffffff | (a<<24));
            }*/
        }
    }
    auto square = SDL_CreateTextureFromSurface(renderer_, square_s);
    setTextureBlendMode(square);
    //setTextureAlphaMod(square, 128);
    SDL_FreeSurface(square_s);
    return square;
}

BP_Texture* Engine::createTextTexture(const std::string& fontname, const std::string& text, int size, BP_Color c) const
{
    auto font = TTF_OpenFont(fontname.c_str(), size);
    if (!font)
    {
        return nullptr;
    }
    //SDL_Color c = { 255, 255, 255, 128 };
    auto text_s = TTF_RenderUTF8_Blended(font, text.c_str(), c);
    auto text_t = SDL_CreateTextureFromSurface(renderer_, text_s);
    SDL_FreeSurface(text_s);
    TTF_CloseFont(font);
    return text_t;
}

int Engine::showMessage(const std::string& content) const
{
    const SDL_MessageBoxButtonData buttons[] = {
        { /* .flags, .buttonid, .text */ 0, 0, "no" },
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "yes" },
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 2, "cancel" },
    };
    const SDL_MessageBoxColorScheme colorScheme = {
        { /* .colors (.r, .g, .b) */
            /* [SDL_MESSAGEBOX_COLOR_BACKGROUND] */
            { 255, 0, 0 },
            /* [SDL_MESSAGEBOX_COLOR_TEXT] */
            { 0, 255, 0 },
            /* [SDL_MESSAGEBOX_COLOR_BUTTON_BORDER] */
            { 255, 255, 0 },
            /* [SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND] */
            { 0, 0, 255 },
            /* [SDL_MESSAGEBOX_COLOR_BUTTON_SELECTED] */
            { 255, 0, 255 } }
    };
    const SDL_MessageBoxData messageboxdata = {
        SDL_MESSAGEBOX_INFORMATION, /* .flags */
        NULL,                       /* .window */
        title_.c_str(),             /* .title */
        content.c_str(),            /* .message */
        SDL_arraysize(buttons),     /* .numbuttons */
        buttons,                    /* .buttons */
        &colorScheme                /* .colorScheme */
    };
    int buttonid;
    SDL_ShowMessageBox(&messageboxdata, &buttonid);
    return buttonid;
}

void Engine::renderSquareTexture(BP_Rect* rect, BP_Color color, uint8_t alpha)
{
    color.a = alpha;
    setColor(square_, color);
    renderCopy(square_, nullptr, rect);
}

int Engine::playVideo(std::string filename)
{
    if (filename == "")
    {
        return 0;
    }
#if defined(_WIN32) && defined(WITH_SMALLPOT) && !defined(_DEBUG)
    return PotInputVideo(tinypot_, (char*)filename.c_str());
#endif
    return 0;
}

int Engine::saveScreen(const char* filename) const
{
    BP_Rect rect;
    rect.x = 0;
    rect.y = 0;
    getWindowSize(rect.w, rect.h);
    BP_Surface* sur = SDL_CreateRGBSurface(0, rect.w, rect.h, 32, RMASK, GMASK, BMASK, AMASK);
    SDL_RenderReadPixels(renderer_, &rect, SDL_PIXELFORMAT_ARGB8888, sur->pixels, rect.w * 4);
    SDL_SaveBMP(sur, filename);
    SDL_FreeSurface(sur);
    return 0;
}

int Engine::saveTexture(BP_Texture* tex, const char* filename) const
{
    BP_Rect rect;
    rect.x = 0;
    rect.y = 0;
    queryTexture(tex, &rect.w, &rect.h);
    setRenderTarget(tex);
    BP_Surface* sur = SDL_CreateRGBSurface(0, rect.w, rect.h, 32, RMASK, GMASK, BMASK, AMASK);
    SDL_RenderReadPixels(renderer_, &rect, SDL_PIXELFORMAT_ARGB8888, sur->pixels, rect.w * 4);
    SDL_SaveBMP(sur, filename);
    SDL_FreeSurface(sur);
    resetRenderTarget();
    return 0;
}
