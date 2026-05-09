#pragma once

#include "Engine.h"
#include "Point.h"
#include "TextureManager.h"

#include <deque>
#include <format>
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
    std::string Text;
    int Size = 15;
    int Frame = 0;
    Color color;
    int Type = 0;
};

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
