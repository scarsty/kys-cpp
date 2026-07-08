#pragma once

#include "ChessBattleEffects.h"

namespace KysChess::Battle
{

enum class BattleEffectRuntimePhase
{
    BattleStart,
    FrameRuntime,
    CastPlanning,
    CastCommit,
    CastFinish,
    HitPreDamage,
    HitDamageModifier,
    HitStatusPayload,
    HitResourcePayload,
    HitFollowUp,
};

struct BattleEffectRuntimeRule
{
    EffectType type;
    Trigger trigger;
    BattleEffectRuntimePhase phase;
    bool selectedSkillMagicAllowed;
    bool ultimateOnly;
};

bool isSelectedSkillMagicAllowed(EffectType type, Trigger trigger);
BattleEffectRuntimePhase runtimePhaseFor(EffectType type, Trigger trigger);
bool isUltimateOnlyMagicEffect(EffectType type, Trigger trigger);

}  // namespace KysChess::Battle
