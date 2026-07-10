#include "Engine.h"
#include "GameUtil.h"
#include "ImGuiLayer.h"
#include "SystemSettings.h"
#include "strfunc.h"
#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
#include <vector>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef __ANDROID__
void notifyAndroidInputReady();
#endif

std::unordered_map<Texture*, Color> Engine::color_cache_;
#ifdef _MSC_VER
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#pragma comment(lib, "user32.lib")
#endif

//#include "opencv4/opencv2/opencv.hpp"

Engine::Engine()
{
}

Engine::~Engine()
{
    destroy();
}

int Engine::init(void* handle /*= nullptr*/, int handle_type /*= 0*/, int maximized, const std::string& str)
{
    if (inited_)
    {
        return 0;
    }
    inited_ = true;
#ifdef _WIN32
    std::string renderer_turn = "direct3d12, opengl, vulkan, direct3d11, direct3d, opengles2, opengles, metal, gpu, software";
    if (!str.empty())
    {
        auto str1 = strfunc::toLowerCase(str);
        for (auto s : strfunc::splitString(renderer_turn, ","))
        {
            if (str1 == strfunc::trim(s))
            {
                renderer_turn = str1;
                break;
            }
        }
    }
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, renderer_turn.c_str());
#endif
#ifdef __ANDROID__
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
#endif
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_PEN_TOUCH_EVENTS, "0");
#endif
#ifndef _WINDLL
    if (!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_SENSOR))
    {
        return -1;
    }
#endif
    if (!PointerInput::instance().initializeActions())
    {
        return -1;
    }
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    SDL_SetEventEnabled(SDL_EVENT_PINCH_BEGIN, false);
    SDL_SetEventEnabled(SDL_EVENT_PINCH_UPDATE, false);
    SDL_SetEventEnabled(SDL_EVENT_PINCH_END, false);
#endif
    window_mode_ = handle_type;
    if (handle)
    {
        if (handle_type == 0)
        {
            //window_ = SDL_CreateWindowFrom(handle);
            Prop props;
            props.set(SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, handle);    //未测试
            SDL_CreateWindowWithProperties(props.id());
        }
        else
        {
            window_ = (Window*)handle;
        }
    }
    else
    {
        Prop props;
        props.set(SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
        props.set(SDL_PROP_WINDOW_CREATE_MAXIMIZED_BOOLEAN, maximized);
        props.set(SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, ui_w_);
        props.set(SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, ui_h_);
        props.set(SDL_PROP_WINDOW_CREATE_TITLE_STRING, title_.c_str());
        window_ = SDL_CreateWindowWithProperties(props.id());
        //window_ = SDL_CreateWindow("Lightning Generator", ui_w_, ui_w_, SDL_WINDOW_RESIZABLE);
    }
    //SDL_CreateWindowFrom()
#ifndef _WINDLL
    SDL_ShowWindow(window_);
    SDL_RaiseWindow(window_);
#endif
    renderer_ = SDL_GetRenderer(window_);
    std::print("{}\n", SDL_GetError());
    if (renderer_ == nullptr)
    {
        Prop props;
        props.set(SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window_);
        renderer_ = SDL_CreateRendererWithProperties(props.id());
        //renderer_ = SDL_CreateRenderer(window_, "Direct3D12");
        renderer_self_ = true;
    }

    std::print("Renderer name: {}\n", SDL_GetRendererName(renderer_));

    //SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");    // SDL2 only; SDL3 uses SDL_SetTextureScaleMode per-texture
    //SDL_EventState(SDL_EVENT_DROP_FILE, SDL_ENABLE);

    //屏蔽触摸板
    //SDL_EventState(SDL_EVENT_FINGER_UP, SDL_DISABLE);
    //SDL_EventState(SDL_EVENT_FINGER_DOWN, SDL_DISABLE);
    //SDL_EventState(SDL_EVENT_FINGER_MOTION, SDL_DISABLE);

    //手柄
    checkGameControllers();

    int num_touch = 0;
    auto touch_devices = SDL_GetTouchDevices(&num_touch);

    std::print("Found {} touch(es)\n", num_touch);
    SDL_free(touch_devices);

    rect_ = { 0, 0, ui_w_, ui_h_ };

    renderPresent();
    TTF_Init();

#ifdef __EMSCRIPTEN__
    // On Emscripten, SDL_GetDisplayBounds returns the tiny CSS viewport
    // (e.g. 320x676 on mobile). Don't let that clamp the game resolution.
    min_x_ = 0;
    min_y_ = 0;
    max_x_ = std::max(ui_w_, 3840);
    max_y_ = std::max(ui_h_, 2160);
#elif defined(_MSC_VER)
    RECT r;
    SystemParametersInfo(SPI_GETWORKAREA, 0, (PVOID)&r, 0);
    int w = GetSystemMetrics(SM_CXEDGE);
    int h = GetSystemMetrics(SM_CYEDGE);
    min_x_ = r.left + w;
    min_y_ = r.top + h + GetSystemMetrics(SM_CYCAPTION);
    max_x_ = r.right - w;
    max_y_ = r.bottom - h;
#else
    Rect r;
    SDL_GetDisplayBounds(0, &r);
    min_x_ = r.x;
    min_y_ = r.y;
    max_x_ = r.w + r.x;
    max_y_ = r.h + r.y;
#endif

    square_ = createRectTexture(100, 100, 0);

    std::print("maximum width and height are: {}, {}\n", max_x_, max_y_);

    createMainTexture(SDL_PixelFormat(0), TEXTUREACCESS_TARGET, ui_w_, ui_h_);
    if (!imgui_)
    {
        imgui_ = std::make_unique<ImGuiLayer>();
    }
    imgui_->init(window_, renderer_);
#ifdef __ANDROID__
    notifyAndroidInputReady();
#endif
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

int Engine::getMaxTextureSize() const
{
    auto props = SDL_GetRendererProperties(renderer_);
    return int(SDL_GetNumberProperty(props, SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 0));
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
    //std::print("{}, {}, {}, {}, {}\n", win_w_, win_h_, w, h, ratio);
    if (!window_)
    {
        return;
    }

    SDL_SetWindowSize(window_, win_w_, win_h_);
    setPresentPosition(tex_);

    SDL_ShowWindow(window_);
    SDL_RaiseWindow(window_);
    SDL_GetWindowSize(window_, &win_w_, &win_h_);
    //std::print("{}, {}, {}, {}, {}\n", win_w_, win_h_, w, h, ratio);
    //resetWindowsPosition();
    //renderPresent();
}

void Engine::setWindowPosition(int x, int y) const
{
    int w, h;
    getWindowSize(w, h);
    if (x == WINDOWPOS_CENTERED)
    {
        x = min_x_ + (max_x_ - min_x_ - w) / 2;
    }
    if (y == WINDOWPOS_CENTERED)
    {
        y = min_y_ + (max_y_ - min_y_ - h) / 2;
    }
    SDL_SetWindowPosition(window_, x, y);
}

void Engine::createMainTexture(PixelFormat pixfmt, TextureAccess a, int w, int h)
{
    if (tex_)
    {
        color_cache_.erase(tex_);
        SDL_DestroyTexture(tex_);
    }
    if (pixfmt < 0)
    {
        tex_ = createRenderedTexture(w, h);
    }
    else
    {
        tex_ = createTexture(pixfmt, a, w, h);
    }
    setPresentPosition(tex_);
}

void Engine::resizeMainTexture(int w, int h) const
{
    float w0, h0;
    uint32_t pix_fmt;
    if (!SDL_GetTextureSize(tex_, &w0, &h0))
    {
        if (int(w0) != w || int(h0) != h)
        {
            //createMainTexture(pix_fmt, w, h);
        }
    }
}

//创建一个专用于画场景的，后期放大
void Engine::createAssistTexture(const std::string& name, int w, int h)
{
    //tex_ = createYUVTexture(w, h);

    auto& tex = tex_map_[name];
    if (tex)
    {
        color_cache_.erase(tex);
        SDL_DestroyTexture(tex);
    }
    int64_t pixfmt = 0;
    SDL_GetNumberProperty(SDL_GetTextureProperties(tex_), SDL_PROP_TEXTURE_FORMAT_NUMBER, pixfmt);
    tex = createTexture((SDL_PixelFormat)pixfmt, TEXTUREACCESS_TARGET, w, h);
    //tex_ = createRenderedTexture(768, 480);
    //SDL_SetTextureBlendMode(tex2_, SDL_BLENDMODE_BLEND);
}

Texture* Engine::cloneTexture(Texture* source) const
{
    if (!source)
    {
        return nullptr;
    }

    int w = 0;
    int h = 0;
    getTextureSize(source, w, h);
    if (w <= 0 || h <= 0)
    {
        return nullptr;
    }

    auto clone = createRenderedTexture(w, h);
    if (!clone)
    {
        return nullptr;
    }

    auto previousTarget = getRenderTarget();
    setRenderTarget(clone);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
    SDL_RenderClear(renderer_);
    SDL_RenderTexture(renderer_, source, nullptr, nullptr);
    setRenderTarget(previousTarget);
    return clone;
}

void Engine::setPresentPosition(Texture* tex, const EngineEvent* sourceEvent)
{
    if (!tex)
    {
        return;
    }
    PresentLayoutInput input;
    getWindowSize(input.windowWidth, input.windowHeight);
    getTextureSize(tex, input.textureWidth, input.textureHeight);
    input.uiWidth = ui_w_;
    input.uiHeight = ui_h_;
    input.keepRatio = keep_ratio_;
    input.rotation = rotation_;
    input.ratioX = ratio_x_;
    input.ratioY = ratio_y_;
    if (input.windowWidth <= 0 || input.windowHeight <= 0
        || input.textureWidth <= 0 || input.textureHeight <= 0
        || input.uiWidth <= 0 || input.uiHeight <= 0)
    {
        return;
    }

    auto geometry = computePresentLayout(input, present_geometry_revision_ + 1);
    if (published_present_geometry_.windowWidth == geometry.windowWidth
        && published_present_geometry_.windowHeight == geometry.windowHeight
        && published_present_geometry_.presentRect.x == geometry.presentRect.x
        && published_present_geometry_.presentRect.y == geometry.presentRect.y
        && published_present_geometry_.presentRect.w == geometry.presentRect.w
        && published_present_geometry_.presentRect.h == geometry.presentRect.h
        && published_present_geometry_.uiWidth == geometry.uiWidth
        && published_present_geometry_.uiHeight == geometry.uiHeight)
    {
        return;
    }
    ++present_geometry_revision_;
    geometry.revision = present_geometry_revision_;
    rect_ = geometry.presentRect;
    published_present_geometry_ = geometry;
    PointerInput::instance().publishPresentGeometry(geometry, sourceEvent);
}

Texture* Engine::createTexture(PixelFormat pix_fmt, TextureAccess a, int w, int h) const
{
    if (pix_fmt == SDL_PIXELFORMAT_UNKNOWN)
    {
        pix_fmt = SDL_PIXELFORMAT_RGBA8888;
    }
    return SDL_CreateTexture(renderer_, pix_fmt, (SDL_TextureAccess)a, w, h);
}

Texture* Engine::createYUVTexture(int w, int h) const
{
    return SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, w, h);
}

void Engine::updateYUVTexture(Texture* t, uint8_t* data0, int size0, uint8_t* data1, int size1, uint8_t* data2, int size2)
{
    SDL_UpdateYUVTexture(t, nullptr, data0, size0, data1, size1, data2, size2);
}

Texture* Engine::createTexture(int w, int h)
{
    return SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, w, h);
}

Texture* Engine::createRenderedTexture(int w, int h) const
{
    return SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
}

void Engine::updateTexture(Texture* t, uint8_t* buffer, int pitch)
{
    SDL_UpdateTexture(t, nullptr, buffer, pitch);
}

int Engine::lockTexture(Texture* t, Rect* r, void** pixel, int* pitch)
{
    return SDL_LockTexture(t, r, pixel, pitch);
}

void Engine::unlockTexture(Texture* t)
{
    SDL_UnlockTexture(t);
}

void Engine::renderPresent() const
{
    //renderMainTextureToWindow();
    SDL_RenderPresent(renderer_);
// #ifndef __EMSCRIPTEN__
//     // On Emscripten with asyncify, clearing here would make the canvas black
//     // during any browser yield (emscripten_sleep, async IO fetch).
//     // renderMainTextureToWindow() already clears before blitting each frame.
//     SDL_RenderClear(renderer_);
// #endif
    //setRenderMainTexture();
}

void Engine::renderTexture(Texture* t /*= nullptr*/, double angle)
{
    FRect rectf;
    SDL_RectToFRect(&rect_, &rectf);
    SDL_RenderTextureRotated(renderer_, t, nullptr, &rectf, angle, nullptr, SDL_FLIP_NONE);
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_PIXELART);
    render_times_++;
}

void Engine::renderTexture(Texture* t, int x, int y, int w, int h, double angle, int inPresent)
{
    if (inPresent == 1)
    {
        x += rect_.x;
        y += rect_.y;
    }
    int w0, h0;
    getTextureSize(t, w0, h0);
    if (w < 0)
    {
        w = w0;
    }
    if (h < 0)
    {
        h = h0;
    }
    Rect r = { x, y, w, h };
    renderTexture(t, nullptr, &r, angle);
}

void Engine::renderTexture(Texture* t, Rect* rect0, Rect* rect1, double angle, int inPresent /*= 0*/)
{
    FRect rect0f, rect1f;
    FRect *rect0f_ptr = nullptr, *rect1f_ptr = nullptr;
    if (rect0)
    {
        SDL_RectToFRect(rect0, &rect0f);
        rect0f_ptr = &rect0f;
    }
    if (rect1)
    {
        SDL_RectToFRect(rect1, &rect1f);
        rect1f_ptr = &rect1f;
    }
    SDL_RenderTextureRotated(renderer_, t, rect0f_ptr, rect1f_ptr, angle, nullptr, SDL_FLIP_NONE);
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_PIXELART);
    render_times_++;
}

void Engine::renderTextureLight(Texture* t, Rect* rect0, Rect* rect1, const std::vector<Color>& colors,
    const std::vector<float>& brightness_v, double angle)
{
    if (!t || !rect1)
    {
        return;
    }

    int w = rect1->w;
    int h = rect1->h;

    int texture_w, texture_h;
    getTextureSize(t, texture_w, texture_h);
    if (w < 0)
    {
        w = texture_w;
    }
    if (h < 0)
    {
        h = texture_h;
    }

    float x0 = float(rect1->x);
    float y0 = float(rect1->y);
    float x1 = x0 + w;
    float y1 = y0 + h;

    float tex_x0 = rect0 ? float(rect0->x) : 0.0f;
    float tex_y0 = rect0 ? float(rect0->y) : 0.0f;
    float tex_x1 = rect0 ? float(rect0->x + rect0->w) : float(texture_w);
    float tex_y1 = rect0 ? float(rect0->y + rect0->h) : float(texture_h);
    const float tw = float((std::max)(1, texture_w));
    const float th = float((std::max)(1, texture_h));

    auto color_to_fcolor = [](const Color& color) -> SDL_FColor
    {
        return { float(color.r) / 255.0f, float(color.g) / 255.0f, float(color.b) / 255.0f, float(color.a) / 255.0f };
    };

    const SDL_FColor default_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    SDL_Vertex vertices[4];

    vertices[0].position = { x0, y0 };
    vertices[0].tex_coord = { tex_x0 / tw, tex_y0 / th };
    vertices[0].color = colors.size() > 0 ? color_to_fcolor(colors[0]) : default_color;

    vertices[1].position = { x1, y0 };
    vertices[1].tex_coord = { tex_x1 / tw, tex_y0 / th };
    vertices[1].color = colors.size() > 1 ? color_to_fcolor(colors[1]) : default_color;

    vertices[2].position = { x1, y1 };
    vertices[2].tex_coord = { tex_x1 / tw, tex_y1 / th };
    vertices[2].color = colors.size() > 2 ? color_to_fcolor(colors[2]) : default_color;

    vertices[3].position = { x0, y1 };
    vertices[3].tex_coord = { tex_x0 / tw, tex_y1 / th };
    vertices[3].color = colors.size() > 3 ? color_to_fcolor(colors[3]) : default_color;

    if (angle != 0)
    {
        const float rad = float(angle * M_PI / 180.0);
        const float cos_angle = float(std::cos(rad));
        const float sin_angle = float(std::sin(rad));
        const float cx = x0 + w * 0.5f;
        const float cy = y0 + h * 0.5f;
        for (auto& vertex : vertices)
        {
            const float rx = vertex.position.x - cx;
            const float ry = vertex.position.y - cy;
            vertex.position.x = cx + rx * cos_angle - ry * sin_angle;
            vertex.position.y = cy + rx * sin_angle + ry * cos_angle;
        }
    }

    int indices[6] = { 0, 1, 2, 2, 3, 0 };
    SDL_RenderGeometry(renderer_, t, vertices, 4, indices, 6);
    render_times_++;

    if (!brightness_v.empty())
    {
        float brightness[4] = { 0, 0, 0, 0 };
        for (int i = 0; i < 4; i++)
        {
            if (i < static_cast<int>(brightness_v.size()))
            {
                brightness[i] = (std::max)(0.0f, brightness_v[i]);
            }
        }

        const float max_brightness = (std::max)((std::max)(brightness[0], brightness[1]), (std::max)(brightness[2], brightness[3]));
        const int full_pass = int(std::floor(max_brightness));
        const float remain = max_brightness - full_pass;

        SDL_BlendMode previous_blend = SDL_BLENDMODE_BLEND;
        SDL_GetTextureBlendMode(t, &previous_blend);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_ADD);

        auto do_add_pass = [&](float pass_base)
        {
            SDL_Vertex add_vertices[4] = { vertices[0], vertices[1], vertices[2], vertices[3] };
            for (int i = 0; i < 4; i++)
            {
                float factor = brightness[i] - pass_base;
                if (factor > 1.0f) { factor = 1.0f; }
                if (factor < 0.0f) { factor = 0.0f; }
                add_vertices[i].color = { factor, factor, factor, 1.0f };
            }
            SDL_RenderGeometry(renderer_, t, add_vertices, 4, indices, 6);
            render_times_++;
        };

        for (int pass = 0; pass < full_pass; pass++)
        {
            do_add_pass(float(pass));
        }
        if (remain > 0.0f)
        {
            do_add_pass(float(full_pass));
        }

        SDL_SetTextureBlendMode(t, previous_blend);
    }

    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_PIXELART);
}

void Engine::renderTexture(Texture* t, Rect* rect0, const std::vector<FPoint>& v, const std::vector<FPoint>& v2)
{
    if (!t || v.size() < 4)
    {
        if (rect0)
        {
            renderTexture(t, rect0, rect0);
        }
        else
        {
            renderTexture(t);
        }
        return;
    }

    float tw = 1.0f;
    float th = 1.0f;
    SDL_GetTextureSize(t, &tw, &th);

    FPoint src[4];
    if (v2.size() >= 4)
    {
        for (int i = 0; i < 4; ++i)
        {
            src[i] = v2[i];
        }
    }
    else if (rect0)
    {
        src[0] = { static_cast<float>(rect0->x), static_cast<float>(rect0->y) };
        src[1] = { static_cast<float>(rect0->x + rect0->w), static_cast<float>(rect0->y) };
        src[2] = { static_cast<float>(rect0->x + rect0->w), static_cast<float>(rect0->y + rect0->h) };
        src[3] = { static_cast<float>(rect0->x), static_cast<float>(rect0->y + rect0->h) };
    }
    else
    {
        src[0] = { 0, 0 };
        src[1] = { tw, 0 };
        src[2] = { tw, th };
        src[3] = { 0, th };
    }

    SDL_Vertex vertices[4];
    for (int i = 0; i < 4; ++i)
    {
        vertices[i].position = v[i];
        vertices[i].tex_coord = { src[i].x / tw, src[i].y / th };
        vertices[i].color = { 1.0f, 1.0f, 1.0f, 1.0f };
    }

    int indices[6] = { 0, 1, 2, 2, 3, 0 };
    SDL_RenderGeometry(renderer_, t, vertices, 4, indices, 6);
    render_times_++;
}

void Engine::renderTextureMesh(Texture* t, const std::vector<FPoint>& v, const std::vector<FPoint>& v2,
    const std::vector<Color>& colors, const std::vector<int>& indices)
{
    if (!t || v.empty() || v.size() != v2.size() || indices.empty())
    {
        return;
    }

    float tw = 1.0f;
    float th = 1.0f;
    SDL_GetTextureSize(t, &tw, &th);

    std::vector<SDL_Vertex> vertices(v.size());
    const bool useVertexColors = colors.size() == v.size();
    for (size_t i = 0; i < v.size(); ++i)
    {
        vertices[i].position = v[i];
        vertices[i].tex_coord = { v2[i].x / tw, v2[i].y / th };
        if (useVertexColors)
        {
            vertices[i].color = {
                colors[i].r / 255.0f,
                colors[i].g / 255.0f,
                colors[i].b / 255.0f,
                colors[i].a / 255.0f
            };
        }
        else
        {
            vertices[i].color = { 1.0f, 1.0f, 1.0f, 1.0f };
        }
    }

    SDL_RenderGeometry(renderer_, t, vertices.data(), static_cast<int>(vertices.size()), indices.data(), static_cast<int>(indices.size()));
    render_times_++;
}

void Engine::destroy()
{
    if (imgui_)
    {
        imgui_->shutdown();
    }
    for (auto& [key, font] : font_cache_)
    {
        TTF_CloseFont(font);
    }
    font_cache_.clear();
    destroyTexture(tex_);
    for (auto& [k, tex] : tex_map_)
    {
        destroyTexture(tex);
    }
    if (renderer_self_)
    {
        SDL_DestroyRenderer(renderer_);
    }
    if (window_mode_ == 0)
    {
        SDL_DestroyWindow(window_);
    }
    PointerInput::instance().clearActionPayloads();

#ifndef _WINDLL
    SDL_Quit();
#endif
}

bool Engine::isFullScreen()
{
    uint32_t state = SDL_GetWindowFlags(window_);
    full_screen_ = (state & SDL_WINDOW_FULLSCREEN);
    return full_screen_;
}

void Engine::toggleFullscreen()
{
    full_screen_ = !full_screen_;
    SDL_SetWindowFullscreen(window_, full_screen_);
    renderClear();
}

Texture* Engine::loadImage(const std::string& filename, int as_white)
{
    //std::print("%s", filename.c_str());
    //屏蔽libpng的错误输出
    DisableStream d(stderr);
    auto sur = IMG_Load(filename.c_str());
    if (!sur)
    {
        LOG("IMG_Load failed: {} ({})\n", filename, SDL_GetError());
        return nullptr;
    }
    if (as_white) { toWhite(sur); }
    auto tex = SDL_CreateTextureFromSurface(renderer_, sur);
    if (!tex)
    {
        LOG("SDL_CreateTextureFromSurface failed: {} ({}x{}, {})\n", filename, sur->w, sur->h, SDL_GetError());
    }
    SDL_DestroySurface(sur);
    return tex;
}

Texture* Engine::loadImageFromMemory(const std::string& content, int as_white) const
{
    auto rw = SDL_IOFromConstMem(content.data(), content.size());
    auto sur = IMG_Load_IO(rw, 1);
    if (!sur)
    {
        LOG("IMG_Load_IO failed: {} bytes ({})\n", content.size(), SDL_GetError());
        return nullptr;
    }
    if (as_white) { toWhite(sur); }
    auto tex = SDL_CreateTextureFromSurface(renderer_, sur);
    if (!tex)
    {
        LOG("SDL_CreateTextureFromSurface failed from memory: {} bytes ({}x{}, {})\n", content.size(), sur->w, sur->h, SDL_GetError());
    }
    SDL_DestroySurface(sur);
    return tex;
}

void Engine::toWhite(Surface* sur)
{
    for (int y = 0; y < sur->h; y++)
    {
        for (int x = 0; x < sur->w; x++)
        {
            uint8_t r, g, b, a;
            if (!SDL_ReadSurfacePixel(sur, x, y, &r, &g, &b, &a))
            {
                continue;
            }
            SDL_WriteSurfacePixel(sur, x, y, 255, 255, 255, a);
        }
    }
}

bool Engine::setKeepRatio(bool b)
{
    keep_ratio_ = b;
    setPresentPosition(tex_);
    return keep_ratio_;
}

void Engine::setUISize(int w, int h)
{
    ui_w_ = w;
    ui_h_ = h;
    setPresentPosition(tex_);
}

double Engine::setRotation(double r)
{
    rotation_ = r;
    setPresentPosition(tex_);
    return rotation_;
}

void Engine::setRatio(double x, double y)
{
    ratio_x_ = x;
    ratio_y_ = y;
    setPresentPosition(tex_);
}

Texture* Engine::transRGBABitmapToTexture(const uint8_t* src, uint32_t color, int w, int h, int stride) const
{
    auto s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA8888);
    SDL_FillSurfaceRect(s, nullptr, color);
    auto p = (uint8_t*)s->pixels;
    for (int x = 0; x < w; x++)
    {
        for (int y = 0; y < h; y++)
        {
            p[4 * (y * w + x)] = src[y * stride + x];
        }
    }
    auto t = SDL_CreateTextureFromSurface(renderer_, s);
    SDL_DestroySurface(s);
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

void Engine::setColor(Texture* tex, Color c)
{
    auto it = color_cache_.find(tex);
    if (it != color_cache_.end() && it->second.r == c.r && it->second.g == c.g && it->second.b == c.b && it->second.a == c.a)
    {
        return;
    }
    color_cache_[tex] = c;
    SDL_SetTextureColorMod(tex, c.r, c.g, c.b);
    setTextureAlphaMod(tex, c.a);
    setTextureBlendMode(tex);
}

void Engine::destroyTexture(Texture* t)
{
    color_cache_.erase(t);
    SDL_DestroyTexture(t);
}

void Engine::fillColor(Color color, int x, int y, int w, int h, BlendMode blend) const
{
    if (w < 0 || h < 0)
    {
        getWindowSize(w, h);
    }
    Rect r{ x, y, w, h };
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_SetRenderDrawBlendMode(renderer_, blend);
    FRect rf;
    SDL_RectToFRect(&r, &rf);
    SDL_RenderFillRect(renderer_, &rf);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
}

void Engine::fillRoundedRect(Color color, int x, int y, int w, int h, int radius, BlendMode blend) const
{
    if (w <= 0 || h <= 0) { return; }
    if (radius < 0) { radius = 0; }
    if (radius > w / 2) { radius = w / 2; }
    if (radius > h / 2) { radius = h / 2; }

    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_SetRenderDrawBlendMode(renderer_, blend);

    // Scanline approach: draw each row exactly once to avoid alpha overlap artifacts
    // Top cap (rows 0..radius-1): curved edges from corner arcs
    for (int row = 0; row < radius; row++)
    {
        int dy = radius - row;
        int dx = (int)std::sqrt((double)(radius * radius - dy * dy));
        float left = (float)(x + radius - dx);
        float right_end = (float)(x + w - radius + dx);
        float row_y = (float)(y + row);
        float row_w = right_end - left;
        if (row_w > 0)
        {
            FRect rf = { left, row_y, row_w, 1.0f };
            SDL_RenderFillRect(renderer_, &rf);
        }
    }
    // Middle band (rows radius..h-radius-1): full width
    if (h - 2 * radius > 0)
    {
        FRect rf = { (float)x, (float)(y + radius), (float)w, (float)(h - 2 * radius) };
        SDL_RenderFillRect(renderer_, &rf);
    }
    // Bottom cap (rows h-radius..h-1): curved edges from corner arcs
    for (int row = 0; row < radius; row++)
    {
        int dy = row + 1;
        int dx = (int)std::sqrt((double)(radius * radius - dy * dy));
        float left = (float)(x + radius - dx);
        float right_end = (float)(x + w - radius + dx);
        float row_y = (float)(y + h - radius + row);
        float row_w = right_end - left;
        if (row_w > 0)
        {
            FRect rf = { left, row_y, row_w, 1.0f };
            SDL_RenderFillRect(renderer_, &rf);
        }
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
}

void Engine::drawRoundedRect(Color color, int x, int y, int w, int h, int radius, BlendMode blend) const
{
    if (w <= 0 || h <= 0) { return; }
    if (radius < 0) { radius = 0; }
    if (radius > w / 2) { radius = w / 2; }
    if (radius > h / 2) { radius = h / 2; }

    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_SetRenderDrawBlendMode(renderer_, blend);

    // Draw four straight edges
    // Top edge
    SDL_RenderLine(renderer_, (float)(x + radius), (float)y, (float)(x + w - radius - 1), (float)y);
    // Bottom edge
    SDL_RenderLine(renderer_, (float)(x + radius), (float)(y + h - 1), (float)(x + w - radius - 1), (float)(y + h - 1));
    // Left edge
    SDL_RenderLine(renderer_, (float)x, (float)(y + radius), (float)x, (float)(y + h - radius - 1));
    // Right edge
    SDL_RenderLine(renderer_, (float)(x + w - 1), (float)(y + radius), (float)(x + w - 1), (float)(y + h - radius - 1));

    // Draw four corner arcs using Bresenham-like circle algorithm
    auto drawCornerArc = [&](int cx, int cy, int quadrant)
    {
        int px = radius, py = 0;
        int d = 1 - radius;
        while (px >= py)
        {
            // Map octant points to the correct quadrant
            switch (quadrant)
            {
            case 0:    // top-left
                SDL_RenderPoint(renderer_, (float)(cx - px), (float)(cy - py));
                SDL_RenderPoint(renderer_, (float)(cx - py), (float)(cy - px));
                break;
            case 1:    // top-right
                SDL_RenderPoint(renderer_, (float)(cx + px), (float)(cy - py));
                SDL_RenderPoint(renderer_, (float)(cx + py), (float)(cy - px));
                break;
            case 2:    // bottom-left
                SDL_RenderPoint(renderer_, (float)(cx - px), (float)(cy + py));
                SDL_RenderPoint(renderer_, (float)(cx - py), (float)(cy + px));
                break;
            case 3:    // bottom-right
                SDL_RenderPoint(renderer_, (float)(cx + px), (float)(cy + py));
                SDL_RenderPoint(renderer_, (float)(cx + py), (float)(cy + px));
                break;
            }
            py++;
            if (d < 0)
            {
                d += 2 * py + 1;
            }
            else
            {
                px--;
                d += 2 * (py - px) + 1;
            }
        }
    };
    drawCornerArc(x + radius, y + radius, 0);                        // top-left
    drawCornerArc(x + w - radius - 1, y + radius, 1);                // top-right
    drawCornerArc(x + radius, y + h - radius - 1, 2);                // bottom-left
    drawCornerArc(x + w - radius - 1, y + h - radius - 1, 3);        // bottom-right

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
}

void Engine::drawAnimatedRoundedRect(Color color, int x, int y, int w, int h, int radius, double phase, int dotCount, double dotLength, BlendMode blend) const
{
    if (w <= 0 || h <= 0) { return; }
    if (radius < 0) { radius = 0; }
    if (radius > w / 2) { radius = w / 2; }
    if (radius > h / 2) { radius = h / 2; }

    // Calculate perimeter segments
    double topLen = w - 2 * radius;
    double rightLen = h - 2 * radius;
    double bottomLen = w - 2 * radius;
    double leftLen = h - 2 * radius;
    double arcLen = 0.5 * M_PI * radius;  // quarter circle arc length
    double totalPerimeter = topLen + rightLen + bottomLen + leftLen + 4 * arcLen;
    if (totalPerimeter <= 0) { return; }

    SDL_SetRenderDrawBlendMode(renderer_, blend);

    // Helper: compute brightness at a given perimeter position
    auto brightness = [&](double pos) -> double
    {
        double frac = pos / totalPerimeter;  // 0..1 position
        double maxBright = 0.0;
        for (int d = 0; d < dotCount; d++)
        {
            double dotPos = std::fmod(phase + (double)d / dotCount, 1.0);
            // Circular distance
            double dist = std::abs(frac - dotPos);
            if (dist > 0.5) dist = 1.0 - dist;
            // Smooth falloff within dotLength
            if (dist < dotLength)
            {
                double t = 1.0 - (dist / dotLength);
                double b = t * t * (3.0 - 2.0 * t);  // smoothstep
                if (b > maxBright) maxBright = b;
            }
        }
        return maxBright;
    };

    // Helper: draw a single pixel with brightness modulation
    auto drawPixel = [&](int px, int py, double perimPos)
    {
        double b = brightness(perimPos);
        uint8_t a = (uint8_t)(color.a * (0.15 + 0.85 * b));  // min 15% alpha
        uint8_t r = (uint8_t)std::min(255.0, color.r * (0.4 + 0.6 * b));
        uint8_t g = (uint8_t)std::min(255.0, color.g * (0.4 + 0.6 * b));
        uint8_t bl = (uint8_t)std::min(255.0, color.b * (0.4 + 0.6 * b));
        SDL_SetRenderDrawColor(renderer_, r, g, bl, a);
        SDL_RenderPoint(renderer_, (float)px, (float)py);
    };

    double pos = 0.0;

    // Segment 1: Top edge (left to right)
    if (topLen > 0)
    {
        int x0 = x + radius, x1 = x + w - radius - 1;
        for (int i = x0; i <= x1; i++)
        {
            double frac = (topLen > 1) ? (double)(i - x0) / (x1 - x0) : 0;
            drawPixel(i, y, pos + frac * topLen);
        }
    }
    pos += topLen;

    // Segment 2: Top-right arc
    {
        int cx = x + w - radius - 1;
        int cy = y + radius;
        // Collect arc points sorted by angle
        std::vector<std::pair<int, int>> arcPoints;
        int px2 = radius, py2 = 0;
        int d = 1 - radius;
        while (px2 >= py2)
        {
            arcPoints.push_back({cx + py2, cy - px2});  // octant 1 → angle ~0 to 45° from top
            arcPoints.push_back({cx + px2, cy - py2});  // octant 2 → angle ~45° to 90° from top
            py2++;
            if (d < 0) { d += 2 * py2 + 1; }
            else { px2--; d += 2 * (py2 - px2) + 1; }
        }
        // Sort by angle (clockwise from top: right direction first)
        std::sort(arcPoints.begin(), arcPoints.end(), [&](auto& a, auto& b) {
            double aa = std::atan2(a.second - cy, a.first - cx);
            double ab = std::atan2(b.second - cy, b.first - cx);
            return aa < ab;
        });
        // Remove duplicates
        arcPoints.erase(std::unique(arcPoints.begin(), arcPoints.end()), arcPoints.end());
        for (int i = 0; i < arcPoints.size(); i++)
        {
            double frac = arcPoints.size() > 1 ? (double)i / (arcPoints.size() - 1) : 0;
            drawPixel(arcPoints[i].first, arcPoints[i].second, pos + frac * arcLen);
        }
    }
    pos += arcLen;

    // Segment 3: Right edge (top to bottom)
    if (rightLen > 0)
    {
        int y0 = y + radius, y1 = y + h - radius - 1;
        for (int i = y0; i <= y1; i++)
        {
            double frac = (rightLen > 1) ? (double)(i - y0) / (y1 - y0) : 0;
            drawPixel(x + w - 1, i, pos + frac * rightLen);
        }
    }
    pos += rightLen;

    // Segment 4: Bottom-right arc
    {
        int cx = x + w - radius - 1;
        int cy = y + h - radius - 1;
        std::vector<std::pair<int, int>> arcPoints;
        int px2 = radius, py2 = 0;
        int d = 1 - radius;
        while (px2 >= py2)
        {
            arcPoints.push_back({cx + px2, cy + py2});
            arcPoints.push_back({cx + py2, cy + px2});
            py2++;
            if (d < 0) { d += 2 * py2 + 1; }
            else { px2--; d += 2 * (py2 - px2) + 1; }
        }
        std::sort(arcPoints.begin(), arcPoints.end(), [&](auto& a, auto& b) {
            double aa = std::atan2(a.second - cy, a.first - cx);
            double ab = std::atan2(b.second - cy, b.first - cx);
            return aa < ab;
        });
        arcPoints.erase(std::unique(arcPoints.begin(), arcPoints.end()), arcPoints.end());
        for (int i = 0; i < arcPoints.size(); i++)
        {
            double frac = arcPoints.size() > 1 ? (double)i / (arcPoints.size() - 1) : 0;
            drawPixel(arcPoints[i].first, arcPoints[i].second, pos + frac * arcLen);
        }
    }
    pos += arcLen;

    // Segment 5: Bottom edge (right to left)
    if (bottomLen > 0)
    {
        int x0 = x + w - radius - 1, x1 = x + radius;
        for (int i = x0; i >= x1; i--)
        {
            double frac = (bottomLen > 1) ? (double)(x0 - i) / (x0 - x1) : 0;
            drawPixel(i, y + h - 1, pos + frac * bottomLen);
        }
    }
    pos += bottomLen;

    // Segment 6: Bottom-left arc
    {
        int cx = x + radius;
        int cy = y + h - radius - 1;
        std::vector<std::pair<int, int>> arcPoints;
        int px2 = radius, py2 = 0;
        int d = 1 - radius;
        while (px2 >= py2)
        {
            arcPoints.push_back({cx - py2, cy + px2});
            arcPoints.push_back({cx - px2, cy + py2});
            py2++;
            if (d < 0) { d += 2 * py2 + 1; }
            else { px2--; d += 2 * (py2 - px2) + 1; }
        }
        // Sort clockwise (for bottom-left: going from bottom to left, angle π/2 to π)
        std::sort(arcPoints.begin(), arcPoints.end(), [&](auto& a, auto& b) {
            double aa = std::atan2(a.second - cy, a.first - cx);
            double ab = std::atan2(b.second - cy, b.first - cx);
            // We need angles in range (0, π), atan2 gives (-π, π)
            if (aa < 0) aa += 2 * M_PI;
            if (ab < 0) ab += 2 * M_PI;
            return aa < ab;
        });
        arcPoints.erase(std::unique(arcPoints.begin(), arcPoints.end()), arcPoints.end());
        for (int i = 0; i < arcPoints.size(); i++)
        {
            double frac = arcPoints.size() > 1 ? (double)i / (arcPoints.size() - 1) : 0;
            drawPixel(arcPoints[i].first, arcPoints[i].second, pos + frac * arcLen);
        }
    }
    pos += arcLen;

    // Segment 7: Left edge (bottom to top)
    if (leftLen > 0)
    {
        int y0 = y + h - radius - 1, y1 = y + radius;
        for (int i = y0; i >= y1; i--)
        {
            double frac = (leftLen > 1) ? (double)(y0 - i) / (y0 - y1) : 0;
            drawPixel(x, i, pos + frac * leftLen);
        }
    }
    pos += leftLen;

    // Segment 8: Top-left arc
    {
        int cx = x + radius;
        int cy = y + radius;
        std::vector<std::pair<int, int>> arcPoints;
        int px2 = radius, py2 = 0;
        int d = 1 - radius;
        while (px2 >= py2)
        {
            arcPoints.push_back({cx - px2, cy - py2});
            arcPoints.push_back({cx - py2, cy - px2});
            py2++;
            if (d < 0) { d += 2 * py2 + 1; }
            else { px2--; d += 2 * (py2 - px2) + 1; }
        }
        // Sort clockwise: angle π to 3π/2 (using atan2 in -π to π range)
        std::sort(arcPoints.begin(), arcPoints.end(), [&](auto& a, auto& b) {
            double aa = std::atan2(a.second - cy, a.first - cx);
            double ab = std::atan2(b.second - cy, b.first - cx);
            // angles in range (-π, -π/2), need descending for clockwise
            return aa > ab;
        });
        arcPoints.erase(std::unique(arcPoints.begin(), arcPoints.end()), arcPoints.end());
        for (int i = 0; i < arcPoints.size(); i++)
        {
            double frac = arcPoints.size() > 1 ? (double)i / (arcPoints.size() - 1) : 0;
            drawPixel(arcPoints[i].first, arcPoints[i].second, pos + frac * arcLen);
        }
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
}

void Engine::initUIStyle()
{
    ui_style_ = GameUtil::getInstance()->getInt("game", "ui_style", 0);
}

void Engine::renderMainTextureToWindow()
{
    resetRenderTarget();
    setPresentPosition(tex_);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    renderTexture(tex_);
    //std::vector<FPoint> v;
    //int w, h;
    //getWindowSize(w, h);
    //float wf = w, hf = h;
    //v.push_back({ wf * 0.25f, 0 });
    //v.push_back({ wf * 0.75f, 0 });
    //v.push_back({ wf, hf });
    //v.push_back({ 0, hf });
    //renderTexture(tex_, v);
}

bool Engine::processImGuiEvent(const EngineEvent& e) const
{
    if (!imgui_)
    {
        return false;
    }
    return imgui_->processEvent(e);
}

bool Engine::processImGuiPrimaryTouch(const TouchSample& sample) const
{
    return imgui_ && imgui_->processPrimaryTouch(sample);
}

void Engine::cancelImGuiPrimaryTouch() const
{
    if (imgui_)
    {
        imgui_->cancelPrimaryTouch();
    }
}

bool Engine::processImGuiApplicationCancel() const
{
    return imgui_ && imgui_->processApplicationCancel();
}

void Engine::renderImGuiOverlay() const
{
    if (!imgui_)
    {
        return;
    }

    int main_texture_w = 0;
    int main_texture_h = 0;
    getTextureSize(tex_, main_texture_w, main_texture_h);
    imgui_->render(window_, renderer_, main_texture_w, main_texture_h, SDL_GetRendererName(renderer_));
}

void Engine::showBattleLogOverlay(const BattleLogViewModel& model, bool respectUserSetting) const
{
    if (!imgui_)
    {
        return;
    }
    if (respectUserSetting && !SystemSettings::getInstance()->data().showBattleLog)
    {
        return;
    }
    imgui_->showBattleLog(model);
}

void Engine::hideBattleLogOverlay() const
{
    if (!imgui_)
    {
        return;
    }
    imgui_->hideBattleLog();
}

bool Engine::isBattleLogOverlayOpen() const
{
    if (!imgui_)
    {
        return false;
    }
    return imgui_->isBattleLogOpen();
}

void Engine::showChangelogWindow(const ChangelogData& data) const
{
    if (!imgui_)
    {
        return;
    }
    imgui_->showChangelog(data);
}

void Engine::hideChangelogWindow() const
{
    if (!imgui_)
    {
        return;
    }
    imgui_->hideChangelog();
}

bool Engine::isChangelogWindowOpen() const
{
    if (!imgui_)
    {
        return false;
    }
    return imgui_->isChangelogOpen();
}

void Engine::createRenderedTexture(const std::string& name, int w, int h)
{
    auto tex = tex_map_[name];
    int w0, h0;
    getTextureSize(tex, w0, h0);
    if (w0 == w && h0 == h)
    {
        return;
    }
    destroyTexture(tex_map_[name]);
    tex_map_[name] = createRenderedTexture(w, h);
    if (!tex_map_[name])
    {
        LOG("createRenderedTexture failed: {} ({}x{}, {})\n", name, w, h, SDL_GetError());
    }
}

void Engine::destroyTexture(const std::string& name)
{
    auto& tex = tex_map_[name];
    destroyTexture(tex);
    tex = nullptr;
}

void Engine::renderTextureToMain(const std::string& name)
{
    setRenderTarget(tex_);
    renderTexture(tex_map_[name], nullptr, nullptr);
    //setRenderTarget(tex_);
    //SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
    //SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
    //SDL_RenderFillRect(renderer_, nullptr);
}

void Engine::mixAudio(uint8_t* dst, const uint8_t* src, uint32_t len, float volume) const
{
    SDL_MixAudio(dst, src, audio_format_, len, 1.0);
    //SDL_PutAudioStreamData(stream_, src, len);
}

int Engine::openAudio(int& freq, int& channels, int& size, int minsize, AudioCallback f)
{
    SDL_AudioSpec want;
    SDL_zero(want);

    std::print("\naudio freq/channels: stream {}/{}, ", freq, channels);
    if (channels <= 2)
    {
        channels = 2;
    }
    want.freq = freq;
    want.format = SDL_AUDIO_F32LE;
    want.channels = channels;

    audio_device_ = 0;
    int i = 10;
    while (audio_device_ == 0 && i > 0)
    {
        auto p_callback = putAudioStreamCallback;
        if (f == nullptr)
        {
            p_callback = nullptr;
        }
        stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &want, p_callback, nullptr);
        audio_device_ = SDL_GetAudioStreamDevice(stream_);
        if (audio_device_)
        {
            break;
        }
        want.channels--;
        i--;
    }
    audio_spec_ = want;
    audio_callback_ = f;
    freq = audio_spec_.freq;
    channels = audio_spec_.channels;
    audio_format_ = audio_spec_.format;

    std::print("device {}/{}\n", audio_spec_.freq, audio_spec_.channels);
    if (audio_device_)
    {
        SDL_ResumeAudioDevice(audio_device_);
    }
    else
    {
        std::print("failed to open audio: {}\n", SDL_GetError());
    }

    return 0;
}

void Engine::putAudioStreamCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount)
{
    if (additional_amount > 0)
    {
        std::vector<uint8_t> data(additional_amount);
        if (getInstance()->audio_callback_)
        {
            getInstance()->audio_callback_(data.data(), additional_amount);
        }
        SDL_PutAudioStreamData(stream, data.data(), additional_amount);
    }
}

int Engine::pollEvent(EngineEvent& e) const
{
    int r = SDL_PollEvent(&e);
    return r;
}

bool Engine::checkKeyPress(Keycode key)
{
    int num = 0;
    SDL_GetKeyboardState(&num);
    auto s = SDL_GetScancodeFromKey(key, nullptr);
    return SDL_GetKeyboardState(&num)[SDL_GetScancodeFromKey(key, nullptr)];
}

bool Engine::gameControllerGetButton(int key)
{
    bool pressed = false;
    if (getTicks() > prev_controller_press_ + interval_controller_press_)
    {
        int i = 0;
        for (auto gc : game_controllers_)
        {
            if (gc)
            {
                if (nintendo_switch_[i])
                {
                    if (key == GAMEPAD_BUTTON_SOUTH) { key = GAMEPAD_BUTTON_EAST; }
                    else if (key == GAMEPAD_BUTTON_EAST) { key = GAMEPAD_BUTTON_SOUTH; }
                    else if (key == GAMEPAD_BUTTON_WEST) { key = GAMEPAD_BUTTON_NORTH; }
                    else if (key == GAMEPAD_BUTTON_NORTH) { key = GAMEPAD_BUTTON_WEST; }
                }
                pressed = SDL_GetGamepadButton(gc, SDL_GamepadButton(key));
                if (pressed)
                {
                    cur_game_controller_ = gc;
                    break;
                }
            }
            i++;
        }
        if (pressed)
        {
            prev_controller_press_ = getTicks();
        }
        interval_controller_press_ = 0;
    }
    return pressed;
}

int16_t Engine::gameControllerGetAxis(int axis)
{
    if (getTicks() > prev_controller_press_ + interval_controller_press_)
    {
        int16_t ret = 0;
        for (auto gc : game_controllers_)
        {
            if (gc)
            {
                ret = SDL_GetGamepadAxis(gc, SDL_GamepadAxis(axis));
            }
            if (ret)
            {
                cur_game_controller_ = gc;
                break;
            }
        }
        if (ret)
        {
            prev_controller_press_ = getTicks();
        }
        interval_controller_press_ = 0;
        if (ret != 0)
        {
            return ret;
        }
    }
    return 0;
}

void Engine::gameControllerRumble(int l, int h, uint32_t time) const
{
    if (cur_game_controller_)
    {
        auto s = SDL_RumbleGamepad(cur_game_controller_, l * 65535 / 100, h * 65535 / 100, time);
    }
}

void Engine::checkGameControllers()
{
    int num_joysticks = 0;
    SDL_GetJoysticks(&num_joysticks);
    if (num_joysticks <= 0)
    {
        std::print("Warning: No joysticks connected!\n");
    }
    else
    {
        //按照游戏控制器打开
        std::print("Found {} game controller(s)\n", num_joysticks);
        game_controllers_.resize(num_joysticks);
        nintendo_switch_.resize(game_controllers_.size());
        for (int i = 0; i < game_controllers_.size(); i++)
        {
            game_controllers_[i] = SDL_OpenGamepad(i);
            if (game_controllers_[i])
            {
                std::string name = SDL_GetGamepadName(game_controllers_[i]);
                std::print("{}\n", name);
                if (name.find("Switch") != std::string::npos) { nintendo_switch_[i] = 1; }
            }
            else
            {
                std::print("Warning: Unable to open game controller! SDL Error: {}\n", SDL_GetError());
            }
        }
    }
}

Texture* Engine::createRectTexture(int w, int h, int style) const
{
    auto square_s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA8888);

    //SDL_FillSurfaceRect(square_s, nullptr, 0xffffffff);
    Rect r = { 0, 0, 1, 1 };
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
                c = 0xffffff00 | a;
            }
            else if (style == 1)
            {
                c = 0xffffffff;
            }
            SDL_FillSurfaceRect(square_s, &r, c);
            /*if ((x - d / 2)*(x - d / 2) + (y - d / 2)*(y - d / 2) < (d / 2) * (d / 2))
            {
                SDL_FillSurfaceRect(square_s, &r, 0x00ffffff | (a<<24));
            }*/
        }
    }
    auto square = SDL_CreateTextureFromSurface(renderer_, square_s);
    setTextureBlendMode(square);
    //setTextureAlphaMod(square, 128);
    SDL_DestroySurface(square_s);
    return square;
}

Texture* Engine::createTextTexture(const std::string& fontname, const std::string& text, int size, Color c)
{
    auto key = std::make_pair(fontname, size);
    auto& font = font_cache_[key];
    if (!font)
    {
        font = TTF_OpenFont(fontname.c_str(), size);
        if (!font)
        {
            font_cache_.erase(key);
            return nullptr;
        }
    }
    auto text_s = TTF_RenderText_Blended(font, text.c_str(), 0, SDL_Color{ c.r, c.g, c.b, c.a });
    auto text_t = SDL_CreateTextureFromSurface(renderer_, text_s);
    SDL_SetTextureScaleMode(text_t, SDL_SCALEMODE_LINEAR);
    SDL_DestroySurface(text_s);
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

void Engine::renderSquareTexture(Rect* rect, Color color, uint8_t alpha)
{
    color.a = alpha;
    setColor(square_, color);
    renderTexture(square_, nullptr, rect);
}

int Engine::saveScreen(const char* filename) const
{
    Rect rect;
    rect.x = 0;
    rect.y = 0;
    getWindowSize(rect.w, rect.h);
    auto sur = SDL_RenderReadPixels(renderer_, &rect);
    SDL_SaveBMP(sur, filename);
    SDL_DestroySurface(sur);
    return 0;
}

int Engine::saveTexture(Texture* tex, const char* filename) const
{
    Rect rect;
    rect.x = 0;
    rect.y = 0;
    getTextureSize(tex, rect.w, rect.h);
    setRenderTarget(tex);
    auto sur = SDL_RenderReadPixels(renderer_, &rect);
    SDL_SaveBMP(sur, filename);
    SDL_DestroySurface(sur);
    resetRenderTarget();
    return 0;
}

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
extern "C" {
EMSCRIPTEN_KEEPALIVE
void request_cancel()
{
    PointerInput::instance().enqueueApplicationCancelAction();
}

EMSCRIPTEN_KEEPALIVE
void notify_fonts_loaded()
{
    EM_ASM({
        if (typeof Module._onCppFontsLoaded === 'function') {
            Module._onCppFontsLoaded();
        }
    });
}

EMSCRIPTEN_KEEPALIVE
void resize_to_viewport(int w, int h, int css_w, int css_h)
{
    auto* eng = Engine::getInstance();
    SDL_SetWindowSize(eng->getWindow(), w, h);
    EM_ASM({
        var c = Module['canvas'];
        c.style.width = $0 + 'px';
        c.style.height = $1 + 'px';
    }, css_w, css_h);
    eng->setPresentPosition(eng->getMainTexture());
}
}
#endif

#ifdef __ANDROID__
#include <jni.h>
extern "C" {
JNIEXPORT void JNICALL
Java_com_kysgame_kyschess_KysActivity_nativeRequestCancel(JNIEnv* env, jclass cls)
{
    PointerInput::instance().enqueueApplicationCancelAction();
}
}

void notifyAndroidInputReady()
{
    auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity)
    {
        return;
    }
    auto activityClass = env->GetObjectClass(activity);
    auto method = env->GetMethodID(activityClass, "onNativeInputReady", "()V");
    if (method)
    {
        env->CallVoidMethod(activity, method);
    }
    env->DeleteLocalRef(activityClass);
    env->DeleteLocalRef(activity);
}
#endif
