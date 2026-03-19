#pragma once

#include <string>
#include <vector>

union SDL_Event;
struct SDL_Renderer;
struct SDL_Window;

struct BattleLogRoleRow
{
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

private:
    bool wantsCaptureEvent(const SDL_Event& event) const;
    void renderBattleLogWindow();

private:
    bool initialized_ = false;
    bool visible_ = false;
    bool show_demo_window_ = false;
    bool show_metrics_window_ = false;
    int battle_log_input_guard_frames_ = 0;
    BattleLogData battle_log_;
};
