#pragma once

#include <string>
#include <vector>

enum class BattleLogEntryTone
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
    SystemAccent,
    DurationValue,
    FrameValue,
    FormulaValue,
    ProjectileId
};

enum class BattleLogEntryCategory
{
    Any,
    Damage,
    Heal,
    Status,
    Cast,
    ProjectileCancel,
    Lifecycle,
    BattleEnd
};

struct BattleLogSegment
{
    std::string text;
    BattleLogFieldTone tone = BattleLogFieldTone::Default;
};

struct BattleLogEntryView
{
    BattleLogEntryTone tone = BattleLogEntryTone::Neutral;
    BattleLogEntryCategory category = BattleLogEntryCategory::Status;
    int sourceId = -1;
    int targetId = -1;
    int sourceTeam = -1;
    int targetTeam = -1;
    std::vector<BattleLogSegment> segments;

    std::string plainText() const;
};

struct BattleLogRoleFilterRow
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

struct BattleLogViewModel
{
    bool open = false;
    std::string title;
    std::string resultText;
    int totalFrames = 0;
    int omittedEntries = 0;
    std::vector<BattleLogRoleFilterRow> allies;
    std::vector<BattleLogRoleFilterRow> enemies;
    std::vector<BattleLogEntryView> entries;
};

struct BattleLogFilter
{
    int allyUnitId = -1;
    int enemyUnitId = -1;
    BattleLogEntryCategory category = BattleLogEntryCategory::Any;
};
