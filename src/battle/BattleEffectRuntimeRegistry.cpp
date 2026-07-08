#include "BattleEffectRuntimeRegistry.h"

#include <array>
#include <cassert>

namespace KysChess::Battle
{
namespace
{

constexpr BattleEffectRuntimeRule rule(
    EffectType type,
    Trigger trigger,
    BattleEffectRuntimePhase phase,
    bool selectedSkillMagicAllowed,
    bool ultimateOnly)
{
    return { type, trigger, phase, selectedSkillMagicAllowed, ultimateOnly };
}

constexpr BattleEffectRuntimeRule selectedUltimateRule(
    EffectType type,
    Trigger trigger,
    BattleEffectRuntimePhase phase)
{
    return rule(type, trigger, phase, true, true);
}

constexpr BattleEffectRuntimeRule comboRuntimeRule(
    EffectType type,
    Trigger trigger,
    BattleEffectRuntimePhase phase)
{
    return rule(type, trigger, phase, false, false);
}

static constexpr std::array kRuntimeRules = {
    selectedUltimateRule(EffectType::CDR, Trigger::Always, BattleEffectRuntimePhase::CastPlanning),

    selectedUltimateRule(EffectType::FlatDmgIncrease, Trigger::Always, BattleEffectRuntimePhase::HitDamageModifier),
    selectedUltimateRule(EffectType::FlatDmgIncrease, Trigger::OnHit, BattleEffectRuntimePhase::HitDamageModifier),
    selectedUltimateRule(EffectType::SkillDmgPct, Trigger::Always, BattleEffectRuntimePhase::HitDamageModifier),
    selectedUltimateRule(EffectType::SkillDmgPct, Trigger::OnHit, BattleEffectRuntimePhase::HitDamageModifier),
    selectedUltimateRule(EffectType::MPRatioDmgBoost, Trigger::Always, BattleEffectRuntimePhase::HitDamageModifier),
    selectedUltimateRule(EffectType::MPRatioDmgBoost, Trigger::OnHit, BattleEffectRuntimePhase::HitDamageModifier),
    selectedUltimateRule(EffectType::PoisonDmgAmp, Trigger::Always, BattleEffectRuntimePhase::HitDamageModifier),
    selectedUltimateRule(EffectType::PoisonDmgAmp, Trigger::OnHit, BattleEffectRuntimePhase::HitDamageModifier),

    selectedUltimateRule(EffectType::ArmorPenChance, Trigger::Always, BattleEffectRuntimePhase::HitPreDamage),
    selectedUltimateRule(EffectType::ArmorPenPct, Trigger::Always, BattleEffectRuntimePhase::HitPreDamage),
    selectedUltimateRule(EffectType::ArmorPen, Trigger::OnHit, BattleEffectRuntimePhase::HitPreDamage),

    selectedUltimateRule(EffectType::MPOnHit, Trigger::Always, BattleEffectRuntimePhase::HitResourcePayload),
    selectedUltimateRule(EffectType::MPOnHit, Trigger::OnHit, BattleEffectRuntimePhase::HitResourcePayload),
    selectedUltimateRule(EffectType::HPOnHit, Trigger::Always, BattleEffectRuntimePhase::HitResourcePayload),
    selectedUltimateRule(EffectType::HPOnHit, Trigger::OnHit, BattleEffectRuntimePhase::HitResourcePayload),
    selectedUltimateRule(EffectType::MPDrain, Trigger::Always, BattleEffectRuntimePhase::HitResourcePayload),
    selectedUltimateRule(EffectType::MPDrain, Trigger::OnHit, BattleEffectRuntimePhase::HitResourcePayload),

    selectedUltimateRule(EffectType::PoisonDOT, Trigger::Always, BattleEffectRuntimePhase::HitStatusPayload),
    selectedUltimateRule(EffectType::PoisonDOT, Trigger::OnHit, BattleEffectRuntimePhase::HitStatusPayload),
    selectedUltimateRule(EffectType::Stun, Trigger::OnHit, BattleEffectRuntimePhase::HitStatusPayload),
    selectedUltimateRule(EffectType::KnockbackChance, Trigger::OnHit, BattleEffectRuntimePhase::HitStatusPayload),
    selectedUltimateRule(EffectType::MPBlock, Trigger::OnHit, BattleEffectRuntimePhase::HitStatusPayload),
    selectedUltimateRule(EffectType::DmgReduceDebuff, Trigger::Always, BattleEffectRuntimePhase::HitStatusPayload),
    selectedUltimateRule(EffectType::DmgReduceDebuff, Trigger::OnHit, BattleEffectRuntimePhase::HitStatusPayload),
    selectedUltimateRule(EffectType::BleedChance, Trigger::Always, BattleEffectRuntimePhase::HitStatusPayload),
    selectedUltimateRule(EffectType::BleedChance, Trigger::OnHit, BattleEffectRuntimePhase::HitStatusPayload),
    selectedUltimateRule(EffectType::OffensiveCharm, Trigger::Always, BattleEffectRuntimePhase::HitStatusPayload),
    selectedUltimateRule(EffectType::OffensiveCharm, Trigger::OnHit, BattleEffectRuntimePhase::HitStatusPayload),
    selectedUltimateRule(EffectType::CharmCDRDebuff, Trigger::Always, BattleEffectRuntimePhase::HitStatusPayload),
    selectedUltimateRule(EffectType::CharmCDRDebuff, Trigger::OnHit, BattleEffectRuntimePhase::HitStatusPayload),
    selectedUltimateRule(EffectType::Execute, Trigger::OnHit, BattleEffectRuntimePhase::HitStatusPayload),

    selectedUltimateRule(EffectType::NearbyTrackingProjectiles, Trigger::OnHit, BattleEffectRuntimePhase::HitFollowUp),
    selectedUltimateRule(EffectType::ProjectileBounce, Trigger::OnHit, BattleEffectRuntimePhase::HitFollowUp),

    selectedUltimateRule(EffectType::PostSkillInvincFrames, Trigger::OnCast, BattleEffectRuntimePhase::CastCommit),
    selectedUltimateRule(EffectType::PostSkillInvincFrames, Trigger::OnUltimate, BattleEffectRuntimePhase::CastCommit),
    selectedUltimateRule(EffectType::UltimateExtraProjectiles, Trigger::OnCast, BattleEffectRuntimePhase::CastCommit),
    selectedUltimateRule(EffectType::UltimateExtraProjectiles, Trigger::OnUltimate, BattleEffectRuntimePhase::CastCommit),
    selectedUltimateRule(EffectType::FlatShield, Trigger::OnCast, BattleEffectRuntimePhase::CastCommit),
    selectedUltimateRule(EffectType::FlatShield, Trigger::OnUltimate, BattleEffectRuntimePhase::CastCommit),
    selectedUltimateRule(EffectType::CurrentHPPctBlast, Trigger::OnCast, BattleEffectRuntimePhase::CastCommit),
    selectedUltimateRule(EffectType::CurrentHPPctBlast, Trigger::OnUltimate, BattleEffectRuntimePhase::CastCommit),
    selectedUltimateRule(EffectType::TeamMPRestore, Trigger::OnCast, BattleEffectRuntimePhase::CastCommit),
    selectedUltimateRule(EffectType::TeamMPRestore, Trigger::OnUltimate, BattleEffectRuntimePhase::CastCommit),
    selectedUltimateRule(EffectType::EnemyMpDamageAll, Trigger::OnCast, BattleEffectRuntimePhase::CastCommit),
    selectedUltimateRule(EffectType::EnemyMpDamageAll, Trigger::OnUltimate, BattleEffectRuntimePhase::CastCommit),
    selectedUltimateRule(EffectType::SpiralBleedProjectile, Trigger::OnCast, BattleEffectRuntimePhase::CastCommit),
    selectedUltimateRule(EffectType::SpiralBleedProjectile, Trigger::OnUltimate, BattleEffectRuntimePhase::CastCommit),
    selectedUltimateRule(EffectType::MPRestore, Trigger::OnCast, BattleEffectRuntimePhase::CastCommit),
    selectedUltimateRule(EffectType::MPRestore, Trigger::OnUltimate, BattleEffectRuntimePhase::CastCommit),
    selectedUltimateRule(EffectType::ControlImmunityFrames, Trigger::OnCast, BattleEffectRuntimePhase::CastCommit),
    selectedUltimateRule(EffectType::ControlImmunityFrames, Trigger::OnUltimate, BattleEffectRuntimePhase::CastCommit),
    selectedUltimateRule(EffectType::LowestAllyHeal, Trigger::OnCast, BattleEffectRuntimePhase::CastCommit),
    selectedUltimateRule(EffectType::LowestAllyHeal, Trigger::OnUltimate, BattleEffectRuntimePhase::CastCommit),

    comboRuntimeRule(EffectType::OnSkillTeamHeal, Trigger::Always, BattleEffectRuntimePhase::CastFinish),
    comboRuntimeRule(EffectType::OnSkillTeamHealPct, Trigger::Always, BattleEffectRuntimePhase::CastFinish),
    selectedUltimateRule(EffectType::OnSkillTeamHeal, Trigger::OnUltimate, BattleEffectRuntimePhase::CastFinish),
    selectedUltimateRule(EffectType::OnSkillTeamHealPct, Trigger::OnUltimate, BattleEffectRuntimePhase::CastFinish),
};

const BattleEffectRuntimeRule* findRuntimeRule(EffectType type, Trigger trigger)
{
    for (const auto& candidate : kRuntimeRules)
    {
        if (candidate.type == type && candidate.trigger == trigger)
        {
            return &candidate;
        }
    }
    return nullptr;
}

}  // namespace

bool isSelectedSkillMagicAllowed(EffectType type, Trigger trigger)
{
    const auto* runtimeRule = findRuntimeRule(type, trigger);
    return runtimeRule && runtimeRule->selectedSkillMagicAllowed;
}

BattleEffectRuntimePhase runtimePhaseFor(EffectType type, Trigger trigger)
{
    const auto* runtimeRule = findRuntimeRule(type, trigger);
    assert(runtimeRule != nullptr);
    return runtimeRule ? runtimeRule->phase : BattleEffectRuntimePhase::BattleStart;
}

bool isUltimateOnlyMagicEffect(EffectType type, Trigger trigger)
{
    const auto* runtimeRule = findRuntimeRule(type, trigger);
    return runtimeRule && runtimeRule->selectedSkillMagicAllowed && runtimeRule->ultimateOnly;
}

}  // namespace KysChess::Battle
