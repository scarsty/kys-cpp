#pragma once

#include "SDL2/SDL.h"
#include "SDL2/SDL_image.h"
#include "SDL2/SDL_ttf.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <string>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "fmt1.h"

#ifndef M_PI
#define M_PI 3.1415926535897
#endif

//这里是底层部分，将SDL的函数均封装了一次
//如需更换底层，则要重新实现下面的全部功能，并重新定义全部常数和类型
#define BP_AUDIO_MIX_MAXVOLUME SDL_MIX_MAXVOLUME

//每个SDL的函数和结构通常仅出现一次，其余的均用已封的功能完成

using AudioCallback = std::function<void(uint8_t*, int)>;
using BP_Renderer = SDL_Renderer;
using BP_Window = SDL_Window;
using BP_Texture = SDL_Texture;
using BP_Rect = SDL_Rect;
using BP_Color = SDL_Color;
using BP_Keycode = SDL_Keycode;
using BP_Surface = SDL_Surface;
using BP_GameController = SDL_GameController;
using BP_Haptic = SDL_Haptic;

enum BP_Align
{
    BP_ALIGN_LEFT,
    BP_ALIGN_MIDDLE,
    BP_ALIGN_RIGHT
};

#define BP_WINDOWPOS_CENTERED SDL_WINDOWPOS_CENTERED

#define RMASK (0xff0000)
#define GMASK (0xff00)
#define BMASK (0xff)
#define AMASK (0xff000000)

//声音类型在其他文件中未使用
using BP_AudioSpec = SDL_AudioSpec;
//这里直接使用SDL的事件结构，如果更换底层需重新实现一套相同的
using BP_Event = SDL_Event;

//这里直接照搬SDL
//更换底层需自己定义一套
//好像是瞎折腾
enum BP_EventType
{
    BP_FIRSTEVENT = SDL_FIRSTEVENT,
    //按关闭按钮
    BP_QUIT = SDL_QUIT,
    //window
    BP_WINDOWEVENT = SDL_WINDOWEVENT,
    BP_SYSWMEVENT = SDL_SYSWMEVENT,
    //键盘
    BP_KEYDOWN = SDL_KEYDOWN,
    BP_KEYUP = SDL_KEYUP,
    BP_TEXTEDITING = SDL_TEXTEDITING,
    BP_TEXTINPUT = SDL_TEXTINPUT,
    //鼠标
    BP_MOUSEMOTION = SDL_MOUSEMOTION,
    BP_MOUSEBUTTONDOWN = SDL_MOUSEBUTTONDOWN,
    BP_MOUSEBUTTONUP = SDL_MOUSEBUTTONUP,
    BP_MOUSEWHEEL = SDL_MOUSEWHEEL,
    //手柄
    BP_JOYAXISMOTION = SDL_JOYAXISMOTION,
    BP_JOYBALLMOTION = SDL_JOYBALLMOTION,
    BP_JOYHATMOTION = SDL_JOYHATMOTION,
    BP_JOYBUTTONDOWN = SDL_JOYBUTTONDOWN,
    BP_JOYBUTTONUP = SDL_JOYBUTTONUP,
    BP_JOYDEVICEADDED = SDL_JOYDEVICEADDED,
    BP_JOYDEVICEREMOVED = SDL_JOYDEVICEREMOVED,
    //游戏控制器
    BP_CONTROLLERAXISMOTION = SDL_CONTROLLERAXISMOTION,
    BP_CONTROLLERBUTTONDOWN = SDL_CONTROLLERBUTTONDOWN,
    BP_CONTROLLERBUTTONUP = SDL_CONTROLLERBUTTONUP,
    BP_CONTROLLERDEVICEADDED = SDL_CONTROLLERDEVICEADDED,
    BP_CONTROLLERDEVICEREMOVED = SDL_CONTROLLERDEVICEREMOVED,
    BP_CONTROLLERDEVICEREMAPPED = SDL_CONTROLLERDEVICEREMAPPED,

    //剪贴板
    BP_CLIPBOARDUPDATE = SDL_CLIPBOARDUPDATE,
    //拖放文件
    BP_DROPFILE = SDL_DROPFILE,
    //渲染改变
    BP_RENDER_TARGETS_RESET = SDL_RENDER_TARGETS_RESET,

    BP_LASTEVENT = SDL_LASTEVENT
};

enum BP_WindowEventID
{
    BP_WINDOWEVENT_NONE = SDL_WINDOWEVENT_NONE, /**< Never used */
    BP_WINDOWEVENT_SHOWN = SDL_WINDOWEVENT_SHOWN,
    BP_WINDOWEVENT_HIDDEN = SDL_WINDOWEVENT_HIDDEN,
    BP_WINDOWEVENT_EXPOSED = SDL_WINDOWEVENT_EXPOSED,

    BP_WINDOWEVENT_MOVED = SDL_WINDOWEVENT_MOVED,

    BP_WINDOWEVENT_RESIZED = SDL_WINDOWEVENT_RESIZED,
    BP_WINDOWEVENT_SIZE_CHANGED = SDL_WINDOWEVENT_SIZE_CHANGED,
    BP_WINDOWEVENT_MINIMIZED = SDL_WINDOWEVENT_MINIMIZED,
    BP_WINDOWEVENT_MAXIMIZED = SDL_WINDOWEVENT_MAXIMIZED,
    BP_WINDOWEVENT_RESTORED = SDL_WINDOWEVENT_RESTORED,

    BP_WINDOWEVENT_ENTER = SDL_WINDOWEVENT_ENTER,
    BP_WINDOWEVENT_LEAVE = SDL_WINDOWEVENT_LEAVE,
    BP_WINDOWEVENT_FOCUS_GAINED = SDL_WINDOWEVENT_FOCUS_GAINED,
    BP_WINDOWEVENT_FOCUS_LOST = SDL_WINDOWEVENT_FOCUS_LOST,
    BP_WINDOWEVENT_CLOSE = SDL_WINDOWEVENT_CLOSE
};

enum BP_KeyBoard
{
    BPK_LEFT = SDLK_LEFT,
    BPK_RIGHT = SDLK_RIGHT,
    BPK_UP = SDLK_UP,
    BPK_DOWN = SDLK_DOWN,
    BPK_SPACE = SDLK_SPACE,
    BPK_ESCAPE = SDLK_ESCAPE,
    BPK_RETURN = SDLK_RETURN,
    BPK_DELETE = SDLK_DELETE,
    BPK_BACKSPACE = SDLK_BACKSPACE,
    BPK_TAB = SDLK_TAB,
    BPK_0 = SDLK_0,
    BPK_1 = SDLK_1,
    BPK_2 = SDLK_2,
    BPK_3 = SDLK_3,
    BPK_4 = SDLK_4,
    BPK_PLUS = SDLK_PLUS,
    BPK_COMMA = SDLK_COMMA,
    BPK_MINUS = SDLK_MINUS,
    BPK_PERIOD = SDLK_PERIOD,
    BPK_EQUALS = SDLK_EQUALS,
    BPK_a = SDLK_a,
    BPK_b = SDLK_b,
    BPK_c = SDLK_c,
    BPK_d = SDLK_d,
    BPK_e = SDLK_e,
    BPK_f = SDLK_f,
    BPK_g = SDLK_g,
    BPK_h = SDLK_h,
    BPK_i = SDLK_i,
    BPK_j = SDLK_j,
    BPK_k = SDLK_k,
    BPK_l = SDLK_l,
    BPK_m = SDLK_m,
    BPK_n = SDLK_n,
    BPK_o = SDLK_o,
    BPK_p = SDLK_p,
    BPK_q = SDLK_q,
    BPK_r = SDLK_r,
    BPK_s = SDLK_s,
    BPK_t = SDLK_t,
    BPK_u = SDLK_u,
    BPK_v = SDLK_v,
    BPK_w = SDLK_w,
    BPK_x = SDLK_x,
    BPK_y = SDLK_y,
    BPK_z = SDLK_z,
    BPK_PAGEUP = SDLK_PAGEUP,
    BPK_PAGEDOWN = SDLK_PAGEDOWN,
};

enum BP_Button
{
    BP_BUTTON_LEFT = SDL_BUTTON_LEFT,
    BP_BUTTON_MIDDLE = SDL_BUTTON_MIDDLE,
    BP_BUTTON_RIGHT = SDL_BUTTON_RIGHT
};

enum BP_State
{
    BP_PRESSED = SDL_PRESSED,
    BP_RELEASED = SDL_RELEASED,
};

enum BP_GameControllerButton
{
    //xbox
    BP_CONTROLLER_BUTTON_INVALID = SDL_CONTROLLER_BUTTON_INVALID,
    BP_CONTROLLER_BUTTON_A = SDL_CONTROLLER_BUTTON_A,
    BP_CONTROLLER_BUTTON_B = SDL_CONTROLLER_BUTTON_B,
    BP_CONTROLLER_BUTTON_X = SDL_CONTROLLER_BUTTON_X,
    BP_CONTROLLER_BUTTON_Y = SDL_CONTROLLER_BUTTON_Y,
    BP_CONTROLLER_BUTTON_BACK = SDL_CONTROLLER_BUTTON_BACK,
    BP_CONTROLLER_BUTTON_GUIDE = SDL_CONTROLLER_BUTTON_GUIDE,
    BP_CONTROLLER_BUTTON_START = SDL_CONTROLLER_BUTTON_START,
    BP_CONTROLLER_BUTTON_LEFTSTICK = SDL_CONTROLLER_BUTTON_LEFTSTICK,
    BP_CONTROLLER_BUTTON_RIGHTSTICK = SDL_CONTROLLER_BUTTON_RIGHTSTICK,
    BP_CONTROLLER_BUTTON_LEFTSHOULDER = SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    BP_CONTROLLER_BUTTON_RIGHTSHOULDER = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
    BP_CONTROLLER_BUTTON_DPAD_UP = SDL_CONTROLLER_BUTTON_DPAD_UP,
    BP_CONTROLLER_BUTTON_DPAD_DOWN = SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    BP_CONTROLLER_BUTTON_DPAD_LEFT = SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    BP_CONTROLLER_BUTTON_DPAD_RIGHT = SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
    BP_CONTROLLER_BUTTON_MISC1 = SDL_CONTROLLER_BUTTON_MISC1,
    BP_CONTROLLER_BUTTON_PADDLE1 = SDL_CONTROLLER_BUTTON_PADDLE1,
    BP_CONTROLLER_BUTTON_PADDLE2 = SDL_CONTROLLER_BUTTON_PADDLE2,
    BP_CONTROLLER_BUTTON_PADDLE3 = SDL_CONTROLLER_BUTTON_PADDLE3,
    BP_CONTROLLER_BUTTON_PADDLE4 = SDL_CONTROLLER_BUTTON_PADDLE4,
    BP_CONTROLLER_BUTTON_TOUCHPAD = SDL_CONTROLLER_BUTTON_TOUCHPAD,
    BP_CONTROLLER_BUTTON_MAX = SDL_CONTROLLER_BUTTON_MAX,
};

enum BP_GameControllerAxis
{
    BP_CONTROLLER_AXIS_INVALID = SDL_CONTROLLER_AXIS_INVALID,
    BP_CONTROLLER_AXIS_LEFTX = SDL_CONTROLLER_AXIS_LEFTX,
    BP_CONTROLLER_AXIS_LEFTY = SDL_CONTROLLER_AXIS_LEFTY,
    BP_CONTROLLER_AXIS_RIGHTX = SDL_CONTROLLER_AXIS_RIGHTX,
    BP_CONTROLLER_AXIS_RIGHTY = SDL_CONTROLLER_AXIS_RIGHTY,
    BP_CONTROLLER_AXIS_TRIGGERLEFT = SDL_CONTROLLER_AXIS_TRIGGERLEFT,
    BP_CONTROLLER_AXIS_TRIGGERRIGHT = SDL_CONTROLLER_AXIS_TRIGGERRIGHT,
    BP_CONTROLLER_AXIS_MAX = SDL_CONTROLLER_AXIS_MAX,
};

enum BP_TextureAccess
{
    BP_TEXTUREACCESS_STATIC = SDL_TEXTUREACCESS_STATIC,       /**< Changes rarely, not lockable */
    BP_TEXTUREACCESS_STREAMING = SDL_TEXTUREACCESS_STREAMING, /**< Changes frequently, lockable */
    BP_TEXTUREACCESS_TARGET = SDL_TEXTUREACCESS_TARGET        /**< Texture can be used as a render target */
};

class Engine
{
private:
    Engine();
    virtual ~Engine();

public:
    static Engine* getInstance()
    {
        static Engine e;
        return &e;
    }
    //图形相关
private:
    bool inited_ = false;
    BP_Window* window_ = nullptr;
    BP_Renderer* renderer_ = nullptr;
    BP_Texture* tex_ = nullptr;
    BP_Texture* tex2_ = nullptr;
    BP_Texture* logo_ = nullptr;
    BP_Rect rect_;
    bool full_screen_ = false;
    bool keep_ratio_ = true;

    int start_w_ = 1024, start_h_ = 640;
    int win_w_, win_h_, min_x_, min_y_, max_x_, max_y_;
    double rotation_ = 0;
    double ratio_x_ = 1, ratio_y_ = 1;

    int render_times_ = 0;

    BP_GameController* game_controller_ = nullptr;
    BP_Haptic* haptic_ = nullptr;

    int switch_ = 0;

    int window_mode_ = 0;    //0-窗口和渲染器自行创建，1-窗口和渲染器由外部创建
    bool renderer_self_ = false;

public:
    int init(void* handle = nullptr, int handle_type = 0, int maximized = 0);

    void getWindowSize(int& w, int& h) const { SDL_GetWindowSize(window_, &w, &h); }
    void getWindowMaxSize(int& w, int& h) const { SDL_GetWindowMaximumSize(window_, &w, &h); }
    int getWindowWidth() const;
    int getWindowHeight() const;
    int getStartWindowWidth() const { return start_w_; }
    int getStartWindowHeight() const { return start_h_; }
    int getMaxWindowWidth() const { return max_x_ - min_x_; }
    int getMaxWindowHeight() const { return max_y_ - min_y_; }
    void getWindowPosition(int& x, int& y) const { SDL_GetWindowPosition(window_, &x, &y); }
    bool getWindowIsMaximized() const { return SDL_GetWindowFlags(window_) & SDL_WINDOW_MAXIMIZED; }
    void setWindowIsMaximized(bool b) const;
    void setWindowSize(int w, int h);
    void setStartWindowSize(int w, int h)
    {
        start_w_ = w;
        start_h_ = h;
    }
    void getStartWindowSize(int& w, int& h) const
    {
        w = start_w_;
        h = start_h_;
    }
    void setWindowPosition(int x, int y) const;
    void setWindowTitle(const std::string& str) const { SDL_SetWindowTitle(window_, str.c_str()); }
    BP_Renderer* getRenderer() const { return renderer_; }

    void createMainTexture(int pixfmt, BP_TextureAccess a, int w, int h);
    void resizeMainTexture(int w, int h) const;
    void createAssistTexture(int w, int h);
    void setPresentPosition(BP_Texture* tex);    //设置贴图的位置
    //void getPresentSize(int& w, int& h) { w = rect_.w; h = rect_.h; }
    int getPresentWidth() const { return rect_.w; }
    int getPresentHeight() const { return rect_.h; }
    BP_Texture* getMainTexture() const { return tex_; }
    void getMainTextureSize(int& w, int& h) const { queryTexture(tex2_, &w, &h); }
    void destroyAssistTexture() const
    {
        if (tex2_)
        {
            destroyTexture(tex2_);
        }
    }
    BP_Texture* createTexture(uint32_t pix_fmt, BP_TextureAccess a, int w, int h) const;
    static void destroyTexture(BP_Texture* t) { SDL_DestroyTexture(t); }
    BP_Texture* createYUVTexture(int w, int h) const;
    static void updateYUVTexture(BP_Texture* t, uint8_t* data0, int size0, uint8_t* data1, int size1, uint8_t* data2, int size2);
    BP_Texture* createARGBTexture(int w, int h);
    BP_Texture* createARGBRenderedTexture(int w, int h);
    static void updateARGBTexture(BP_Texture* t, uint8_t* buffer, int pitch);
    static int lockTexture(BP_Texture* t, BP_Rect* r, void** pixel, int* pitch);
    static void unlockTexture(BP_Texture* t);
    void showLogo() { renderCopy(logo_, nullptr, nullptr); }
    void renderPresent() const;
    void renderClear() const { SDL_RenderClear(renderer_); }
    static void setTextureAlphaMod(BP_Texture* t, uint8_t alpha) { SDL_SetTextureAlphaMod(t, alpha); }
    static void queryTexture(BP_Texture* t, int* w, int* h) { SDL_QueryTexture(t, nullptr, nullptr, w, h); }
    void setRenderTarget(BP_Texture* t) const { SDL_SetRenderTarget(renderer_, t); }
    BP_Texture* getRenderTarget() const { return SDL_GetRenderTarget(renderer_); }
    void resetRenderTarget() const { setRenderTarget(nullptr); }
    static void createWindow() {}
    static void createRenderer() {}
    void renderCopy(BP_Texture* t = nullptr, double angle = 0);
    void renderCopy(BP_Texture* t, int x, int y, int w = 0, int h = 0, double angle = 0, int inPresent = 0);
    void renderCopy(BP_Texture* t, BP_Rect* rect0, BP_Rect* rect1, double angle = 0, int inPresent = 0);
    void destroy() const;
    bool isFullScreen();
    void toggleFullscreen();
    BP_Texture* loadImage(const std::string& filename, int as_white = 0);
    BP_Texture* loadImageFromMemory(const std::string& content, int as_white = 0) const;
    static void toWhite(BP_Surface* sur);
    bool setKeepRatio(bool b);
    BP_Texture* transBitmapToTexture(const uint8_t* src, uint32_t color, int w, int h, int stride) const;
    double setRotation(double r) { return rotation_ = r; }
    void resetWindowPosition();
    void setRatio(double x, double y)
    {
        ratio_x_ = x;
        ratio_y_ = y;
    }
    static void setColor(BP_Texture* tex, BP_Color c);
    void fillColor(BP_Color color, int x, int y, int w, int h) const;
    void setRenderMainTexture() const { setRenderTarget(tex_); }
    void renderMainTextureToWindow();
    void setRenderAssistTexture() const { setRenderTarget(tex2_); }
    void renderAssistTextureToMain();
    static int setTextureBlendMode(BP_Texture* t) { return SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND); }

    void resetRenderTimes(int t = 0) { render_times_ = t; }
    int getRenderTimes() const { return render_times_; }

    BP_Texture* getRenderAssistTexture() const { return tex2_; }

    //声音相关
private:
    SDL_AudioDeviceID audio_device_;
    AudioCallback audio_callback_ = nullptr;
    SDL_AudioFormat audio_format_ = AUDIO_S16;
    BP_AudioSpec audio_spec_;

public:
    void pauseAudio(int pause) const { SDL_PauseAudioDevice(audio_device_, pause); }
    void closeAudio() const { SDL_CloseAudioDevice(audio_device_); };
    static int getMaxVolume() { return BP_AUDIO_MIX_MAXVOLUME; };
    void mixAudio(Uint8* dst, const Uint8* src, Uint32 len, int volume) const;
    SDL_AudioFormat getAudioFormat() const { return audio_format_; }
    int openAudio(int& freq, int& channels, int& size, int minsize, AudioCallback f);
    static void mixAudioCallback(void* userdata, Uint8* stream, int len);
    void setAudioCallback(AudioCallback cb = nullptr) { audio_callback_ = cb; }

    //事件相关
private:
    uint64_t time_;

public:
    static void delay(double t) { std::this_thread::sleep_for(std::chrono::nanoseconds(int64_t(t * 1e6))); }
    static double getTicks()
    {
        static auto start = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()).time_since_epoch().count();
        return 1e-6 * (std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()).time_since_epoch().count() - start);
    }
    uint64_t tic() { return time_ = getTicks(); }
    void toc()
    {
        if (getTicks() != time_)
        {
            fmt1::print("{}\n", getTicks() - time_);
        }
    }
    static void getMouseState(int& x, int& y);
    void getMouseStateInStartWindow(int& x, int& y) const;
    void setMouseState(int x, int y) const;
    void setMouseStateInStartWindow(int x, int y) const;
    int pollEvent(BP_Event& e) const;
    static int pollEvent() { return SDL_PollEvent(nullptr); }
    static int pushEvent(BP_Event& e) { return SDL_PushEvent(&e); }
    static void flushEvent() { SDL_FlushEvent(0); }
    static void free(void* mem) { SDL_free(mem); }
    static bool checkKeyPress(BP_Keycode key);
    bool gameControllerGetButton(int key) const;
    int16_t gameControllerGetAxis(int axis) const;

    void gameControllerRumble(int l, int h, uint32_t time) const;

    //UI相关
private:
    BP_Texture* square_ = nullptr;

public:
    BP_Texture* createRectTexture(int w, int h, int style) const;
    BP_Texture* createTextTexture(const std::string& fontname, const std::string& text, int size, BP_Color c) const;
    int showMessage(const std::string& content) const;
    void renderSquareTexture(BP_Rect* rect, BP_Color color, uint8_t alpha);

public:
    //标题;
    std::string title_;

private:
    void* tinypot_ = nullptr;

public:
    static int playVideo(std::string filename);
    int saveScreen(const char* filename) const;
    int saveTexture(BP_Texture* tex, const char* filename) const;

    //输入相关
    static void startTextInput() { SDL_StartTextInput(); }
    static void stopTextInput() { SDL_StopTextInput(); }
    static void setTextInputRect(int x, int y, int w = 0, int h = 0)
    {
        BP_Rect r = { x, y, w, h };
        SDL_SetTextInputRect(&r);
    }
};
