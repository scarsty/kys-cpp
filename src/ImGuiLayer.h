#pragma once

#include "BattleLogImGuiView.h"
#include "PointerInput.h"

#include <string>
#include <vector>

union SDL_Event;
struct SDL_Renderer;
struct SDL_Window;

struct ChangelogLine
{
    int headingLevel = 0;
    int bulletIndent = -1;
    std::string text;
};

struct ChangelogData
{
    bool open = false;
    std::string title;
    std::string sourcePath;
    std::string error;
    std::vector<ChangelogLine> lines;
};

struct ImGuiPrimaryTouchRoute
{
    bool forwardToBackend{};
    bool consumed{};
};

class ImGuiPrimaryTouchRouting
{
public:
    ImGuiPrimaryTouchRoute route(TouchPhase phase, bool guarded)
    {
        if (phase == TouchPhase::Down)
        {
            state_ = guarded ? State::Suppressed : State::Forwarded;
        }
        const auto state = state_;
        if (phase == TouchPhase::Up || phase == TouchPhase::Canceled)
        {
            state_ = State::None;
        }
        return {
            state == State::Forwarded,
            state == State::Suppressed,
        };
    }

    bool cancelNeedsBackendRelease()
    {
        const bool release = state_ == State::Forwarded;
        state_ = State::None;
        return release;
    }

private:
    enum class State { None, Forwarded, Suppressed };
    State state_ = State::None;
};

class ImGuiLayer
{
public:
    void init(SDL_Window* window, SDL_Renderer* renderer);
    void shutdown();

    bool processEvent(const SDL_Event& event);
    bool processPrimaryTouch(const TouchSample& sample);
    void cancelPrimaryTouch();
    bool processApplicationCancel();
    void render(SDL_Window* window, SDL_Renderer* renderer, int main_texture_w, int main_texture_h, const char* renderer_name);
    void showBattleLog(const BattleLogViewModel& model);
    void hideBattleLog();
    bool isBattleLogOpen() const;
    void showChangelog(const ChangelogData& data);
    void hideChangelog();
    bool isChangelogOpen() const;

private:
    bool wantsCaptureEvent(const SDL_Event& event) const;
    bool isOverlayInputGuardActive() const;
    bool processPrimaryTouchBackendEvent(const SDL_Event& event);
    void renderBattleLogWindow();
    void renderChangelogWindow();

private:
    bool initialized_ = false;
    bool visible_ = false;
    bool show_metrics_window_ = false;
    int changelog_input_guard_frames_ = 0;
    bool changelog_hover_guard_ = false;
    bool changelog_dragging_ = false;
    BattleLogWindowState battle_log_;
    BattleLogImGuiView battle_log_view_;
    ChangelogData changelog_;
    ImGuiPrimaryTouchRouting primary_touch_routing_;
    SDL_FPoint primary_touch_position_{};
};
