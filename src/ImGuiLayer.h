#pragma once

#include <string>
#include <vector>

union SDL_Event;
struct SDL_Renderer;
struct SDL_Window;

struct BattleLogRoleRow
{
    int id = -1;
    std::string name;
    int team = 0;
    int damageDealt = 0;
    int damageTaken = 0;
    int kills = 0;
    int cancelDmg = 0;
    int hpRemaining = 0;
    int maxHp = 0;
    bool dead = false;
};

enum class BattleLogTone
{
    Neutral,
    Ally,
    Enemy,
    System
};

enum class BattleLogFieldTone
{
    Default,
    AllyName,
    EnemyName,
    SkillName,
    DamageValue,
    SystemAccent
};

struct BattleLogSegment
{
    std::string text;
    BattleLogFieldTone tone = BattleLogFieldTone::Default;
};

struct BattleLogLine
{
    std::string text;
    BattleLogTone tone = BattleLogTone::Neutral;
    int sourceId = -1;
    int targetId = -1;
    int sourceTeam = -1;
    int targetTeam = -1;
    std::vector<BattleLogSegment> segments;
};

struct BattleLogData
{
    bool open = false;
    std::string title;
    std::string resultText;
    int totalFrames = 0;
    int omittedEntries = 0;
    std::vector<BattleLogRoleRow> allies;
    std::vector<BattleLogRoleRow> enemies;
    std::vector<BattleLogLine> entries;
};

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
    void showBattleLog(const BattleLogData& data);
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
    int battle_log_input_guard_frames_ = 0;
    int system_menu_input_guard_frames_ = 0;
    int changelog_input_guard_frames_ = 0;
    bool battle_log_hover_guard_ = false;
    bool system_menu_hover_guard_ = false;
    bool changelog_hover_guard_ = false;
    bool battle_log_dragging_ = false;
    bool changelog_dragging_ = false;
    int battle_log_child_flip_ = 0;
    int battle_log_ally_filter_id_ = -1;
    int battle_log_enemy_filter_id_ = -1;
    BattleLogData battle_log_;
    BattleSystemMenuData system_menu_;
    ChangelogData changelog_;
};
