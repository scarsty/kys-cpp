#include "Engine.h"
#ifdef _MSC_VER
#define NOMINMAX
#include <windows.h>
#pragma comment(lib, "user32.lib")
#endif

Engine::Engine()
{
}

Engine::~Engine()
{
    destroy();
}

BP_Texture* Engine::createYUVTexture(int w, int h)
{
    return SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, w, h);
}

void Engine::updateYUVTexture(BP_Texture* t, uint8_t* data0, int size0, uint8_t* data1, int size1, uint8_t* data2, int size2)
{
    SDL_UpdateYUVTexture(testTexture(t), nullptr, data0, size0, data1, size1, data2, size2);
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
    SDL_UpdateTexture(testTexture(t), nullptr, buffer, pitch);
}

void Engine::renderCopy(BP_Texture* t, int x, int y, int w, int h, int inPresent)
{
    if (inPresent == 1)
    {
        x += rect_.x;
        y += rect_.y;
    }
    BP_Rect r = { x, y, w, h };
    renderCopy(t, nullptr, &r);
}

void Engine::renderCopy(BP_Texture* t /*= nullptr*/)
{
    SDL_RenderCopyEx(renderer_, testTexture(t), nullptr, &rect_, rotation_, nullptr, SDL_FLIP_NONE);
    render_times_++;
}

void Engine::renderCopy(BP_Texture* t, BP_Rect* rect1, double angle)
{
    SDL_RenderCopyEx(renderer_, t, nullptr, rect1, angle, nullptr, SDL_FLIP_NONE);
    render_times_++;
}

void Engine::renderCopy(BP_Texture* t, BP_Rect* rect0, BP_Rect* rect1, int inPresent /*= 0*/)
{
    SDL_RenderCopy(renderer_, t, rect0, rect1);
    render_times_++;
}

void Engine::destroy()
{
    //SDL_DestroyTexture(tex_);
    destroyAssistTexture();
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
#if defined(_WIN32) && defined(WITH_SMALLPOT)
    PotDestory(tinypot_);
#endif
    SDL_Quit();
}

bool Engine::checkKeyPress(BP_Keycode key)
{
    return SDL_GetKeyboardState(NULL)[SDL_GetScancodeFromKey(key)];
}

BP_Texture* Engine::createSquareTexture(int size)
{
    int d = size;
    auto square_s = SDL_CreateRGBSurface(0, d, d, 32, RMASK, GMASK, BMASK, AMASK);

    //SDL_FillRect(square_s, nullptr, 0xffffffff);
    BP_Rect r = { 0, 0, 1, 1 };
    auto& x = r.x;
    auto& y = r.y;
    uint8_t a = 0;
    for (x = 0; x < d; x++)
    {
        for (y = 0; y < d; y++)
        {
            a = 100 + 150 * cos(3.14159265358979323846 * (1.0 * y / d - 0.5));
            auto c = 0x00ffffff | (a << 24);
            SDL_FillRect(square_s, &r, c);
            /*if ((x - d / 2)*(x - d / 2) + (y - d / 2)*(y - d / 2) < (d / 2) * (d / 2))
            {
                SDL_FillRect(square_s, &r, 0x00ffffff | (a<<24));
            }*/
        }
    }
    square_ = SDL_CreateTextureFromSurface(renderer_, square_s);
    setTextureBlendMode(square_);
    setTextureAlphaMod(square_, 128);
    SDL_FreeSurface(square_s);
    return square_;
}

BP_Texture* Engine::createTextTexture(const std::string& fontname, const std::string& text, int size, BP_Color c)
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

int Engine::init(void* handle)
{
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
    {
        return -1;
    }

    window_ = SDL_CreateWindow(title_.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        start_w_, start_h_, SDL_WINDOW_RESIZABLE);

    SDL_ShowWindow(window_);
    SDL_RaiseWindow(window_);
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE /*| SDL_RENDERER_PRESENTVSYNC*/);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    //屏蔽触摸板
    SDL_EventState(SDL_FINGERUP, SDL_DISABLE);
    SDL_EventState(SDL_FINGERDOWN, SDL_DISABLE);
    SDL_EventState(SDL_FINGERMOTION, SDL_DISABLE);

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

    square_ = createSquareTexture(100);

    fmt::print("maximum width and height are: {}, {}\n", max_x_, max_y_);
#if defined(_WIN32) && defined(WITH_SMALLPOT)
    tinypot_ = PotCreateFromWindow(window_);
#endif
    return 0;
}

int Engine::getWindowWidth()
{
    int w, h;
    getWindowSize(w, h);
    return w;
}

int Engine::getWindowHeight()
{
    int w, h;
    getWindowSize(w, h);
    return h;
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

BP_Texture* Engine::loadImage(const std::string& filename)
{
    //fmt::print("%s", filename.c_str());
    return IMG_LoadTexture(renderer_, filename.c_str());
}

BP_Texture* Engine::loadImageFromMemory(const std::string& content)
{
    auto rw = SDL_RWFromConstMem(content.data(), content.size());
    return IMG_LoadTextureTyped_RW(renderer_, rw, 1, "png");
}

bool Engine::setKeepRatio(bool b)
{
    return keep_ratio_ = b;
}

//创建一个专用于画场景的，后期放大
void Engine::createAssistTexture(int w, int h)
{
    //tex_ = createYUVTexture(w, h);
    tex2_ = createARGBRenderedTexture(w, h);
    //tex_ = createARGBRenderedTexture(768, 480);
    setPresentPosition();
}

void Engine::setPresentPosition()
{
    if (!tex_)
    {
        return;
    }
    int w_dst = 0, h_dst = 0;
    int w_src = 0, h_src = 0;
    getWindowSize(w_dst, h_dst);
    queryTexture(tex_, &w_src, &h_src);
    w_src *= ratio_x_;
    h_src *= ratio_y_;
    if (keep_ratio_)
    {
        if (w_src == 0 || h_src == 0)
        {
            return;
        }
        double w_ratio = 1.0 * w_dst / w_src;
        double h_ratio = 1.0 * h_dst / h_src;
        double ratio = std::min(w_ratio, h_ratio);
        if (w_ratio > h_ratio)
        {
            //宽度大，左右留空
            rect_.x = (w_dst - w_src * ratio) / 2;
            rect_.y = 0;
            rect_.w = w_src * ratio;
            rect_.h = h_dst;
        }
        else
        {
            //高度大，上下留空
            rect_.x = 0;
            rect_.y = (h_dst - h_src * ratio) / 2;
            rect_.w = w_dst;
            rect_.h = h_src * ratio;
        }
    }
    else
    {
        rect_.x = 0;
        rect_.y = 0;
        rect_.w = w_dst;
        rect_.h = h_dst;
    }
}

BP_Texture* Engine::transBitmapToTexture(const uint8_t* src, uint32_t color, int w, int h, int stride)
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

int Engine::showMessage(const std::string& content)
{
    const SDL_MessageBoxButtonData buttons[] =
    {
        { /* .flags, .buttonid, .text */ 0, 0, "no" },
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "yes" },
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 2, "cancel" },
    };
    const SDL_MessageBoxColorScheme colorScheme =
    {
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
            { 255, 0, 255 }
        }
    };
    const SDL_MessageBoxData messageboxdata =
    {
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

void Engine::setWindowSize(int w, int h)
{
    if (w <= 0 || h <= 0)
    {
        return;
    }
    win_w_ = std::min(max_x_ - min_x_, w);
    win_h_ = std::min(max_y_ - min_y_, h);
    SDL_SetWindowSize(window_, win_w_, win_h_);
    setPresentPosition();
    getWindowSize(win_w_, win_h_);
    //resetWindowsPosition();
    //renderPresent();
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

void Engine::fillColor(BP_Color color, int x, int y, int w, int h)
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

void Engine::renderAssistTextureToWindow()
{
    resetRenderTarget();
    renderCopy(tex2_, nullptr, nullptr);
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
#if defined(_WIN32) && defined(WITH_SMALLPOT)
    return PotInputVideo(tinypot_, (char*)filename.c_str());
#endif
    return 0;
}

int Engine::saveScreen(const char* filename)
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

int Engine::saveTexture(BP_Texture* tex, const char* filename)
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

void Engine::setWindowPosition(int x, int y)
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
