#pragma once

#ifndef __ANDROID__
#include "SDL3/SDL.h"
#include "SDL3_image/SDL_image.h"
#include "SDL3_ttf/SDL_ttf.h"
#else
#include "SDL.h"
#include "SDL_image.h"
#include "SDL_ttf.h"
#endif

#include <algorithm>
#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <unordered_map>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "fmt1.h"

#ifndef M_PI
#define M_PI 3.1415926535897
#endif

//这里是底层部分，将SDL的函数均封装了一次
//如需更换底层，则要重新实现下面的全部功能，并重新定义全部常数和类型

//每个SDL的函数和结构通常仅出现一次，其余的均用别名

using AudioCallback = std::function<void(uint8_t*, int)>;
using Renderer = SDL_Renderer;
using Window = SDL_Window;
using Texture = SDL_Texture;
using Rect = SDL_Rect;
using Color = SDL_Color;
using Keycode = SDL_Keycode;
using Surface = SDL_Surface;
using Gamepad = SDL_Gamepad;
using Haptic = SDL_Haptic;

enum Align
{
    ALIGN_LEFT,
    ALIGN_MIDDLE,
    ALIGN_RIGHT
};

#define BP_WINDOWPOS_CENTERED SDL_WINDOWPOS_CENTERED

#define RMASK (0xff0000)
#define GMASK (0xff00)
#define BMASK (0xff)
#define AMASK (0xff000000)

//声音类型在其他文件中未使用
using AudioSpec = SDL_AudioSpec;
//这里直接使用SDL的事件结构
using EngineEvent = SDL_Event;

//这里直接照搬SDL
//更换底层需自己定义一套
//好像是瞎折腾
enum EventType
{
    EVENT_FIRST = SDL_EVENT_FIRST,
    //按关闭按钮
    EVENT_QUIT = SDL_EVENT_QUIT,
    //app事件
    EVENT_DID_ENTER_BACKGROUND = SDL_EVENT_DID_ENTER_BACKGROUND,
    EVENT_WILL_ENTER_FOREGROUND = SDL_EVENT_WILL_ENTER_FOREGROUND,
    //window
    //BP_WINDOWEVENT = SDL_WINDOWEVENT,
    //BP_SYSWMEVENT = SDL_SYSWMEVENT,
    //键盘
    EVENT_KEY_DOWN = SDL_EVENT_KEY_DOWN,
    EVENT_KEY_UP = SDL_EVENT_KEY_UP,
    EVENT_TEXT_EDITING = SDL_EVENT_TEXT_EDITING,
    EVENT_TEXT_INPUT = SDL_EVENT_TEXT_INPUT,
    //鼠标
    EVENT_MOUSE_MOTION = SDL_EVENT_MOUSE_MOTION,
    EVENT_MOUSE_BUTTON_DOWN = SDL_EVENT_MOUSE_BUTTON_DOWN,
    EVENT_MOUSE_BUTTON_UP = SDL_EVENT_MOUSE_BUTTON_UP,
    EVENT_MOUSE_WHEEL = SDL_EVENT_MOUSE_WHEEL,
    //手柄
    EVENT_JOYSTICK_AXIS_MOTION = SDL_EVENT_JOYSTICK_AXIS_MOTION,
    EVENT_JOYSTICK_BALL_MOTION = SDL_EVENT_JOYSTICK_BALL_MOTION,
    EVENT_JOYSTICK_HAT_MOTION = SDL_EVENT_JOYSTICK_HAT_MOTION,
    EVENT_JOYSTICK_BUTTON_DOWN = SDL_EVENT_JOYSTICK_BUTTON_DOWN,
    EVENT_JOYSTICK_BUTTON_UP = SDL_EVENT_JOYSTICK_BUTTON_UP,
    EVENT_JOYSTICK_ADDED = SDL_EVENT_JOYSTICK_ADDED,
    EVENT_JOYSTICK_REMOVED = SDL_EVENT_JOYSTICK_REMOVED,
    //游戏控制器
    EVENT_GAMEPAD_AXIS_MOTION = SDL_EVENT_GAMEPAD_AXIS_MOTION,
    EVENT_GAMEPAD_BUTTON_DOWN = SDL_EVENT_GAMEPAD_BUTTON_DOWN,
    EVENT_GAMEPAD_BUTTON_UP = SDL_EVENT_GAMEPAD_BUTTON_UP,
    EVENT_GAMEPAD_ADDED = SDL_EVENT_GAMEPAD_ADDED,
    EVENT_GAMEPAD_REMOVED = SDL_EVENT_GAMEPAD_REMOVED,
    EVENT_GAMEPAD_REMAPPED = SDL_EVENT_GAMEPAD_REMAPPED,
    //触摸
    EVENT_FINGER_DOWN = SDL_EVENT_FINGER_DOWN,
    EVENT_FINGER_UP = SDL_EVENT_FINGER_UP,
    EVENT_FINGER_MOTION = SDL_EVENT_FINGER_MOTION,

    //剪贴板
    EVENT_CLIPBOARD_UPDATE = SDL_EVENT_CLIPBOARD_UPDATE,
    //拖放文件
    EVENT_DROP_FILE = SDL_EVENT_DROP_FILE,
    //渲染改变
    EVENT_RENDER_TARGETS_RESET = SDL_EVENT_RENDER_TARGETS_RESET,

    EVENT_LAST = SDL_EVENT_LAST
};

enum BP_WindowEventID
{
    EVENT_WINDOW_SHOWN = SDL_EVENT_WINDOW_SHOWN,
    EVENT_WINDOW_HIDDEN = SDL_EVENT_WINDOW_HIDDEN,
    EVENT_WINDOW_EXPOSED = SDL_EVENT_WINDOW_EXPOSED,

    EVENT_WINDOW_MOVED = SDL_EVENT_WINDOW_MOVED,

    EVENT_WINDOW_RESIZED = SDL_EVENT_WINDOW_RESIZED,
    EVENT_WINDOW_PIXEL_SIZE_CHANGED = SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED,
    EVENT_WINDOW_MINIMIZED = SDL_EVENT_WINDOW_MINIMIZED,
    EVENT_WINDOW_MAXIMIZED = SDL_EVENT_WINDOW_MAXIMIZED,
    EVENT_WINDOW_RESTORED = SDL_EVENT_WINDOW_RESTORED,

    EVENT_WINDOW_MOUSE_ENTER = SDL_EVENT_WINDOW_MOUSE_ENTER,
    EVENT_WINDOW_MOUSE_LEAVE = SDL_EVENT_WINDOW_MOUSE_LEAVE,
    EVENT_WINDOW_FOCUS_GAINED = SDL_EVENT_WINDOW_FOCUS_GAINED,
    EVENT_WINDOW_FOCUS_LOST = SDL_EVENT_WINDOW_FOCUS_LOST,
    EVENT_WINDOW_CLOSE_REQUESTED = SDL_EVENT_WINDOW_CLOSE_REQUESTED
};

enum KeyBoard
{
    K_LEFT = SDLK_LEFT,
    K_RIGHT = SDLK_RIGHT,
    K_UP = SDLK_UP,
    K_DOWN = SDLK_DOWN,
    K_SPACE = SDLK_SPACE,
    K_ESCAPE = SDLK_ESCAPE,
    K_RETURN = SDLK_RETURN,
    K_DELETE = SDLK_DELETE,
    K_BACKSPACE = SDLK_BACKSPACE,
    K_TAB = SDLK_TAB,
    K_0 = SDLK_0,
    K_1 = SDLK_1,
    K_2 = SDLK_2,
    K_3 = SDLK_3,
    K_4 = SDLK_4,
    K_PLUS = SDLK_PLUS,
    K_COMMA = SDLK_COMMA,
    K_MINUS = SDLK_MINUS,
    K_PERIOD = SDLK_PERIOD,
    K_EQUALS = SDLK_EQUALS,
    K_A = SDLK_A,
    K_B = SDLK_B,
    K_C = SDLK_C,
    K_D = SDLK_D,
    K_E = SDLK_E,
    K_F = SDLK_F,
    K_G = SDLK_G,
    K_H = SDLK_H,
    K_I = SDLK_I,
    K_J = SDLK_J,
    K_K = SDLK_K,
    K_L = SDLK_L,
    K_M = SDLK_M,
    K_N = SDLK_N,
    K_O = SDLK_O,
    K_P = SDLK_P,
    K_Q = SDLK_Q,
    K_R = SDLK_R,
    K_S = SDLK_S,
    K_T = SDLK_T,
    K_U = SDLK_U,
    K_V = SDLK_V,
    K_W = SDLK_W,
    K_X = SDLK_X,
    K_Y = SDLK_Y,
    K_Z = SDLK_Z,
    K_PAGEUP = SDLK_PAGEUP,
    K_PAGEDOWN = SDLK_PAGEDOWN,
};

enum MouseButton
{
    BUTTON_LEFT = SDL_BUTTON_LEFT,
    BUTTON_MIDDLE = SDL_BUTTON_MIDDLE,
    BUTTON_RIGHT = SDL_BUTTON_RIGHT
};

//enum State
//{
//    PRESSED = SDL_PRESSED,
//    RELEASED = SDL_RELEASED,
//};

enum BP_GameControllerButton
{
    //xbox
    GAMEPAD_BUTTON_INVALID = SDL_GAMEPAD_BUTTON_INVALID,
    GAMEPAD_BUTTON_SOUTH = SDL_GAMEPAD_BUTTON_SOUTH,
    GAMEPAD_BUTTON_EAST = SDL_GAMEPAD_BUTTON_EAST,
    GAMEPAD_BUTTON_WEST = SDL_GAMEPAD_BUTTON_WEST,
    GAMEPAD_BUTTON_NORTH = SDL_GAMEPAD_BUTTON_NORTH,
    GAMEPAD_BUTTON_BACK = SDL_GAMEPAD_BUTTON_BACK,
    GAMEPAD_BUTTON_GUIDE = SDL_GAMEPAD_BUTTON_GUIDE,
    GAMEPAD_BUTTON_START = SDL_GAMEPAD_BUTTON_START,
    GAMEPAD_BUTTON_LEFT_STICK = SDL_GAMEPAD_BUTTON_LEFT_STICK,
    GAMEPAD_BUTTON_RIGHT_STICK = SDL_GAMEPAD_BUTTON_RIGHT_STICK,
    GAMEPAD_BUTTON_LEFT_SHOULDER = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
    GAMEPAD_BUTTON_RIGHT_SHOULDER = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    GAMEPAD_BUTTON_DPAD_UP = SDL_GAMEPAD_BUTTON_DPAD_UP,
    GAMEPAD_BUTTON_DPAD_DOWN = SDL_GAMEPAD_BUTTON_DPAD_DOWN,
    GAMEPAD_BUTTON_DPAD_LEFT = SDL_GAMEPAD_BUTTON_DPAD_LEFT,
    GAMEPAD_BUTTON_DPAD_RIGHT = SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
    GAMEPAD_BUTTON_MISC1 = SDL_GAMEPAD_BUTTON_MISC1,
    GAMEPAD_BUTTON_RIGHT_PADDLE1 = SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,
    GAMEPAD_BUTTON_LEFT_PADDLE1 = SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,
    GAMEPAD_BUTTON_RIGHT_PADDLE2 = SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2,
    GAMEPAD_BUTTON_LEFT_PADDLE2 = SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,
    GAMEPAD_BUTTON_TOUCHPAD = SDL_GAMEPAD_BUTTON_TOUCHPAD,
    GAMEPAD_BUTTON_COUNT = SDL_GAMEPAD_BUTTON_COUNT,
};

enum BP_GameControllerAxis
{
    GAMEPAD_AXIS_INVALID = SDL_GAMEPAD_AXIS_INVALID,
    GAMEPAD_AXIS_LEFTX = SDL_GAMEPAD_AXIS_LEFTX,
    GAMEPAD_AXIS_LEFTY = SDL_GAMEPAD_AXIS_LEFTY,
    GAMEPAD_AXIS_RIGHTX = SDL_GAMEPAD_AXIS_RIGHTX,
    GAMEPAD_AXIS_RIGHTY = SDL_GAMEPAD_AXIS_RIGHTY,
    GAMEPAD_AXIS_LEFT_TRIGGER = SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
    GAMEPAD_AXIS_RIGHT_TRIGGER = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
    GAMEPAD_AXIS_COUNT = SDL_GAMEPAD_AXIS_COUNT,
};

enum BP_TextureAccess
{
    TEXTUREACCESS_STATIC = SDL_TEXTUREACCESS_STATIC,       /**< Changes rarely, not lockable */
    TEXTUREACCESS_STREAMING = SDL_TEXTUREACCESS_STREAMING, /**< Changes frequently, lockable */
    TEXTUREACCESS_TARGET = SDL_TEXTUREACCESS_TARGET        /**< Texture can be used as a render target */
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
    Window* window_ = nullptr;
    Renderer* renderer_ = nullptr;
    Texture* tex_ = nullptr;
    Texture* tex2_ = nullptr;
    Texture* logo_ = nullptr;
    Rect rect_;
    bool full_screen_ = false;
    bool keep_ratio_ = true;

    int start_w_ = 1024, start_h_ = 640;
    int win_w_, win_h_, min_x_, min_y_, max_x_, max_y_;
    double rotation_ = 0;
    double ratio_x_ = 1, ratio_y_ = 1;

    int render_times_ = 0;

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

    Renderer* getRenderer() const { return renderer_; }

    void createMainTexture(int pixfmt, BP_TextureAccess a, int w, int h);
    void resizeMainTexture(int w, int h) const;
    void createAssistTexture(int w, int h);
    void setPresentPosition(Texture* tex);    //设置贴图的位置

    //void getPresentSize(int& w, int& h) { w = rect_.w; h = rect_.h; }
    int getPresentWidth() const { return rect_.w; }

    int getPresentHeight() const { return rect_.h; }

    Texture* getMainTexture() const { return tex_; }

    void getMainTextureSize(int& w, int& h) const { queryTexture(tex2_, &w, &h); }

    void destroyAssistTexture() const
    {
        if (tex2_)
        {
            destroyTexture(tex2_);
        }
    }

    Texture* createTexture(uint32_t pix_fmt, BP_TextureAccess a, int w, int h) const;

    static void destroyTexture(Texture* t) { SDL_DestroyTexture(t); }

    Texture* createYUVTexture(int w, int h) const;
    static void updateYUVTexture(Texture* t, uint8_t* data0, int size0, uint8_t* data1, int size1, uint8_t* data2, int size2);
    Texture* createARGBTexture(int w, int h);
    Texture* createARGBRenderedTexture(int w, int h);
    static void updateARGBTexture(Texture* t, uint8_t* buffer, int pitch);
    static int lockTexture(Texture* t, Rect* r, void** pixel, int* pitch);
    static void unlockTexture(Texture* t);

    void showLogo() { renderTexture(logo_, nullptr, nullptr); }

    void renderPresent() const;

    void renderClear() const { SDL_RenderClear(renderer_); }

    static void setTextureAlphaMod(Texture* t, uint8_t alpha) { SDL_SetTextureAlphaMod(t, alpha); }

    static void queryTexture(Texture* t, int* w, int* h) 
    {
        float wf, hf;
        SDL_GetTextureSize(t, &wf, &hf);
        *w = wf;
        *h = hf;
    }

    void setRenderTarget(Texture* t) const { SDL_SetRenderTarget(renderer_, t); }

    Texture* getRenderTarget() const { return SDL_GetRenderTarget(renderer_); }

    void resetRenderTarget() const { setRenderTarget(nullptr); }

    static void createWindow() {}

    static void createRenderer() {}

    void renderTexture(Texture* t = nullptr, double angle = 0);
    void renderTexture(Texture* t, int x, int y, int w = 0, int h = 0, double angle = 0, int inPresent = 0);
    void renderTexture(Texture* t, Rect* rect0, Rect* rect1, double angle = 0, int inPresent = 0);
    void destroy() const;
    bool isFullScreen();
    void toggleFullscreen();
    Texture* loadImage(const std::string& filename, int as_white = 0);
    Texture* loadImageFromMemory(const std::string& content, int as_white = 0) const;
    static void toWhite(Surface* sur);
    bool setKeepRatio(bool b);
    Texture* transBitmapToTexture(const uint8_t* src, uint32_t color, int w, int h, int stride) const;

    double setRotation(double r) { return rotation_ = r; }

    void resetWindowPosition();

    void setRatio(double x, double y)
    {
        ratio_x_ = x;
        ratio_y_ = y;
    }

    static void setColor(Texture* tex, Color c);
    void fillColor(Color color, int x, int y, int w, int h) const;

    void setRenderMainTexture() const { setRenderTarget(tex_); }

    void renderMainTextureToWindow();

    void setRenderAssistTexture() const { setRenderTarget(tex2_); }

    void renderAssistTextureToMain();

    static int setTextureBlendMode(Texture* t) { return SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND); }

    void resetRenderTimes(int t = 0) { render_times_ = t; }

    int getRenderTimes() const { return render_times_; }

    Texture* getRenderAssistTexture() const { return tex2_; }

    //声音相关
private:
    SDL_AudioDeviceID audio_device_;
    AudioCallback audio_callback_ = nullptr;
    SDL_AudioFormat audio_format_ = SDL_AUDIO_F32LE;
    AudioSpec audio_spec_;

public:
    void pauseAudio(int pause) const
    {
        if (pause)
        {
            SDL_PauseAudioDevice(audio_device_);
        }
        else
        {
            SDL_ResumeAudioDevice(audio_device_);
        }
    }

    void closeAudio() const { SDL_CloseAudioDevice(audio_device_); }

    static float getMaxVolume() { return 1.0; }

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
    int pollEvent(EngineEvent& e) const;

    static int pollEvent() { return SDL_PollEvent(nullptr); }

    static int pushEvent(EngineEvent& e) { return SDL_PushEvent(&e); }

    static void flushEvent() { SDL_FlushEvent(0); }

    static void free(void* mem) { SDL_free(mem); }

    static bool checkKeyPress(Keycode key);

private:
    std::vector<Gamepad*> game_controllers_;
    std::vector<int> nintendo_switch_;
    Gamepad* cur_game_controller_ = nullptr;
    double prev_controller_press_ = 0;
    double interval_controller_press_ = 0;
    std::unordered_map<int, int> virtual_stick_button_;
    int16_t virtual_stick_axis_[6] = { 0 };

public:
    bool gameControllerGetButton(int key);
    int16_t gameControllerGetAxis(int axis);
    void gameControllerRumble(int l, int h, uint32_t time) const;

    void setInterValControllerPress(double t) { interval_controller_press_ = t; }

    void setGameControllerButton(int key, int value) { virtual_stick_button_[key] = value; }

    void setGameControllerAxis(int axis, int16_t value) { virtual_stick_axis_[axis] = value; }

    void clearGameControllerButton()
    {
        for (auto& i : virtual_stick_button_)
        {
            i.second = 0;
        }
    }

    void clearGameControllerAxis()
    {
        for (auto& i : virtual_stick_axis_)
        {
            i = 0;
        }
    }

    void checkGameControllers();

    //UI相关
private:
    Texture* square_ = nullptr;

public:
    Texture* createRectTexture(int w, int h, int style) const;
    Texture* createTextTexture(const std::string& fontname, const std::string& text, int size, Color c) const;
    int showMessage(const std::string& content) const;
    void renderSquareTexture(Rect* rect, Color color, uint8_t alpha);

public:
    //标题;
    std::string title_;

private:
    void* tinypot_ = nullptr;

public:
    static int playVideo(std::string filename);
    int saveScreen(const char* filename) const;
    int saveTexture(Texture* tex, const char* filename) const;

    //输入相关
    void startTextInput() { SDL_StartTextInput(window_); }

    void stopTextInput() { SDL_StopTextInput(window_); }

    void setTextInputArea(int x, int y, int w = 0, int h = 0)
    {
        Rect r = { x, y, w, h };
        SDL_SetTextInputArea(window_, &r, 0);
    }
};
