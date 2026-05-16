#pragma once

#include "BattleLogImGuiView.h"

#include <string>
#include <vector>

union SDL_Event;
struct SDL_Renderer;
struct SDL_Window;

struct BattleSystemMenuData
{
    bool open = false;
    bool positionSwapEnabled = false;
    int musicVolume = 50;
    int soundVolume = 50;
    bool manualCamera = false;
    int battleSpeed = 1;
    bool simplifiedChinese = true;
    bool showBattleLog = true;
};

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

class ImGuiLayer
{
public:
    void init(SDL_Window* window, SDL_Renderer* renderer);
    void shutdown();

    bool processEvent(const SDL_Event& event);
    void render(SDL_Window* window, SDL_Renderer* renderer, int main_texture_w, int main_texture_h, const char* renderer_name);
    void showBattleLog(const BattleLogViewModel& model);
    void hideBattleLog();
    bool isBattleLogOpen() const;
    void showBattleSystemMenu(const BattleSystemMenuData& data);
    void hideBattleSystemMenu();
    bool isBattleSystemMenuOpen() const;
    BattleSystemMenuData getBattleSystemMenuData() const;
    void showChangelog(const ChangelogData& data);
    void hideChangelog();
    bool isChangelogOpen() const;

private:
    bool wantsCaptureEvent(const SDL_Event& event) const;
    void renderBattleLogWindow();
    void renderBattleSystemMenuWindow();
    void renderChangelogWindow();

private:
    bool initialized_ = false;
    bool visible_ = false;
    bool show_metrics_window_ = false;
    int system_menu_input_guard_frames_ = 0;
    int changelog_input_guard_frames_ = 0;
    bool system_menu_hover_guard_ = false;
    bool changelog_hover_guard_ = false;
    bool changelog_dragging_ = false;
    BattleLogWindowState battle_log_;
    BattleLogImGuiView battle_log_view_;
    BattleSystemMenuData system_menu_;
    ChangelogData changelog_;
};
