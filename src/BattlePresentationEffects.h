#pragma once

#include "Engine.h"
#include "Point.h"
#include "TextureManager.h"

#include <algorithm>
#include <cassert>
#include <deque>
#include <format>
#include <optional>
#include <string>

struct BattleAttackEffect
{
    Pointf Pos;
    Pointf Velocity;
    Pointf Acceleration;
    int Frame = 0;
    int TotalFrame = 1;
    int TotalEffectFrame = 1;
    std::string Path;
    int FollowUnitId = -1;
    int VisualAttackId = -1;
    int VisualEffectId = -1;
    int VisualOnly = 0;
    int VisualTeam = -1;
    int Through = 0;
    int SpiralMotion = 0;
    Pointf SpiralCenter;
    float SpiralRadius = 0.0f;
    float SpiralRadiusGrowth = 0.0f;
    float SpiralAngle = 0.0f;
    float SpiralAngularVelocity = 0.0f;

    void setEft(int num)
    {
        VisualEffectId = num;
        setPath(std::format("eft/eft{:03}", num));
    }

    void setPath(const std::string& path)
    {
        Path = path;
        TotalEffectFrame = TextureManager::getInstance()->getTextureGroupCount(Path);
    }

    int renderTeam() const
    {
        return VisualTeam;
    }
};

struct BattleTextEffect
{
    Pointf Pos;
    std::optional<Pointf> PaperAnchor;
    std::string Text;
    int Size = 15;
    int Frame = 0;
    int PaperFollowUnitId = -1;
    Color color;
    int Type = 0;
    float PaperScreenOffsetX{};
};

inline constexpr int BattleRoleEchoInitialAlpha = 255;
inline constexpr float BattleRoleEchoOffset = 36.0f;
inline constexpr int BattleRoleEchoLingeringFrames = 4;

inline Color battleRoleEchoTint()
{
    return { 160, 225, 255, 255 };
}

struct BattleRoleEchoEffect
{
    int SourceUnitId = -1;
    int TargetUnitId = -1;
    int ActType = -1;
    int Frame{};
    int TotalFrame = 1;
    int Alpha = BattleRoleEchoInitialAlpha;
    float Offset = BattleRoleEchoOffset;
};

inline int battleRoleEchoRenderAlpha(const BattleRoleEchoEffect& effect)
{
    assert(effect.TotalFrame > 0);
    if (effect.TotalFrame == 1)
    {
        return effect.Alpha;
    }
    const int frame = std::clamp(effect.Frame, 0, effect.TotalFrame - 1);
    return effect.Alpha * (effect.TotalFrame - 1 - frame) / (effect.TotalFrame - 1);
}

inline void advanceBattleVisualOnlyEffects(std::deque<BattleAttackEffect>& effects)
{
    for (auto& effect : effects)
    {
        if (effect.VisualOnly)
        {
            ++effect.Frame;
        }
    }
}

inline void advanceBattlePresentationEffects(std::deque<BattleAttackEffect>& effects, bool battleFrameAdvanced)
{
    if (!battleFrameAdvanced)
    {
        return;
    }

    advanceBattleVisualOnlyEffects(effects);
    std::erase_if(effects, [](const BattleAttackEffect& effect)
        {
            return effect.VisualOnly && effect.Frame >= effect.TotalFrame;
        });
}

inline void advanceBattleRoleEchoEffects(
    std::deque<BattleRoleEchoEffect>& effects,
    bool battleFrameAdvanced)
{
    if (!battleFrameAdvanced)
    {
        return;
    }
    for (auto& effect : effects)
    {
        ++effect.Frame;
    }
    std::erase_if(effects, [](const BattleRoleEchoEffect& effect)
        {
            return effect.Frame >= effect.TotalFrame;
        });
}
