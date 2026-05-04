#include "battle/BattleHitResolver.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace KysChess::Battle;

namespace
{

KysChess::AppliedEffectInstance triggeredEffect(KysChess::EffectType type,
                                                KysChess::Trigger trigger,
                                                int value,
                                                int triggerValue = 0,
                                                int duration = 0,
                                                int maxCount = 0)
{
    KysChess::AppliedEffectInstance effect;
    effect.type = type;
    effect.trigger = trigger;
    effect.value = value;
    effect.triggerValue = triggerValue;
    effect.duration = duration;
    effect.maxCount = maxCount;
    return effect;
}

BattleHitResolutionInput hitInput()
{
    BattleHitResolutionInput input;
    input.attackEvent.type = BattleAttackEventType::Hit;
    input.attackEvent.sourceUnitId = 1;
    input.attackEvent.unitId = 2;
    input.attackEvent.frame = 0;
    input.attackEvent.totalFrame = 10;
    input.attackEvent.position = { 1.0f, 0.0f, 0.0f };
    input.attackEvent.operationType = BattleOperationType::Melee;
    input.attackEvent.strengthMultiplier = 1.0f;
    input.attacker.id = 1;
    input.attacker.hp = 80;
    input.attacker.maxHp = 100;
    input.attacker.mp = 50;
    input.attacker.maxMp = 100;
    input.attacker.position = { -1.0f, 0.0f, 0.0f };
    input.defender.id = 2;
    input.defender.hp = 100;
    input.defender.maxHp = 100;
    input.defender.mp = 20;
    input.defender.maxMp = 100;
    input.defender.position = { 0.0f, 0.0f, 0.0f };
    input.defender.facing = { 1.0f, 0.0f, 0.0f };
    return input;
}

const BattleAcceptedHitSideEffectCommand* firstAcceptedHitCommand(const BattleHitResolutionResult& result)
{
    auto it = std::find_if(result.commands.begin(), result.commands.end(), [](const BattleGameplayCommand& command)
        {
            return std::holds_alternative<BattleAcceptedHitSideEffectCommand>(command);
        });
    return it == result.commands.end() ? nullptr : &std::get<BattleAcceptedHitSideEffectCommand>(*it);
}

const BattleHpDamageCommand* firstHpDamageCommand(const BattleHitResolutionResult& result)
{
    auto it = std::find_if(result.commands.begin(), result.commands.end(), [](const BattleGameplayCommand& command)
        {
            return std::holds_alternative<BattleHpDamageCommand>(command);
        });
    return it == result.commands.end() ? nullptr : &std::get<BattleHpDamageCommand>(*it);
}

}  // namespace

TEST_CASE("BattleHitResolver_NonHitEvent_ReturnsNoGameplayCommands", "[battle][hit_resolver][unit]")
{
    BattleHitResolutionInput input;
    input.attackEvent.type = BattleAttackEventType::Moved;
    input.attacker.id = 1;
    input.defender.id = 2;

    auto result = BattleHitResolver().resolve(input);

    CHECK(result.commands.empty());
    CHECK(result.presentationEvents.empty());
}

TEST_CASE("BattleHitResolver_HiddenWeaponUsesResolvedItemDamageDividedByFive", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = -1;
    input.item.id = 501;
    input.item.resolvedDamage = 100;

    auto result = BattleHitResolver().resolve(input);

    CHECK(result.shapedHpDamage == Catch::Approx(20.0));
    CHECK(result.finalHpDamage == 20);
}

TEST_CASE("BattleHitResolver_MagicUsesResolvedBaseDamage", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.resolvedBaseDamage = 70;

    auto result = BattleHitResolver().resolve(input);

    CHECK(result.shapedHpDamage == Catch::Approx(70.0));
    CHECK(result.finalHpDamage == 70);
}

TEST_CASE("BattleHitResolver_ProjectCancelAndStrengthShapeFinalDamage", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.attackEvent.projectileCancelDamage = 10;
    input.attackEvent.strengthMultiplier = 2.0f;
    input.attackEvent.frame = 5;
    input.attackEvent.operationType = BattleOperationType::TrackingProjectile;
    input.skill.id = 101;
    input.skill.resolvedBaseDamage = 100;

    auto result = BattleHitResolver().resolve(input);

    CHECK(result.shapedHpDamage == Catch::Approx(229.5));
    CHECK(result.finalHpDamage == 229);
}

TEST_CASE("BattleHitResolver_FrozenSideEffectEmitsAcceptedHitCommand", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.attackEvent.operationType = BattleOperationType::Dash;
    input.attackEvent.totalFrame = 20;
    input.skill.id = 101;
    input.skill.resolvedBaseDamage = 90;

    auto result = BattleHitResolver().resolve(input);

    const auto* command = std::get_if<BattleAcceptedHitSideEffectCommand>(&result.commands.front());
    REQUIRE(command);
    CHECK(command->sourceUnitId == 1);
    CHECK(command->targetUnitId == 2);
    CHECK(command->damage.acceptedHit);
    CHECK(command->damage.frozenFrames == 5);
}

TEST_CASE("BattleHitResolver_KnockbackIsReturnedAsCommand", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.resolvedBaseDamage = 90;

    auto result = BattleHitResolver().resolve(input);

    auto knockback = std::find_if(result.commands.begin(), result.commands.end(), [](const BattleGameplayCommand& command)
        {
            return std::holds_alternative<BattleKnockbackCommand>(command);
        });
    REQUIRE(knockback != result.commands.end());
    const auto& command = std::get<BattleKnockbackCommand>(*knockback);
    CHECK(command.targetUnitId == 2);
    CHECK(command.velocityDelta.x == Catch::Approx(2.0f));
    CHECK(command.velocityDelta.y == Catch::Approx(0.0f));
    CHECK(command.velocityCap == Catch::Approx(3.0));
    CHECK(command.grantHurtFrame);
}

TEST_CASE("BattleHitResolver_CritMarksResultAndEmitsStatusLog", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.resolvedBaseDamage = 50;
    input.attackerCombo.critChancePct = 100;
    input.attackerCombo.critMultiplier = 200;
    input.percentRolls = { 0.0 };

    auto result = BattleHitResolver().resolve(input);

    CHECK(result.critical);
    CHECK(result.shapedHpDamage == Catch::Approx(100.0));
    REQUIRE(result.presentationEvents.size() == 1);
    CHECK(result.presentationEvents[0].type == BattlePresentationEventType::StatusLog);
    CHECK(result.presentationEvents[0].sourceUnitId == 1);
    CHECK(result.presentationEvents[0].targetUnitId == 2);
    CHECK(result.presentationEvents[0].text == "暴擊（200%）");
}

TEST_CASE("BattleHitResolver_HpOnHitEmitsHealPresentationAndAcceptedHitCommand", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.resolvedBaseDamage = 50;
    input.attackerCombo.hpOnHit = 30;

    auto result = BattleHitResolver().resolve(input);

    const auto* command = firstAcceptedHitCommand(result);
    REQUIRE(command);
    CHECK(command->sourceUnitId == 1);
    CHECK(command->targetUnitId == 2);
    CHECK(command->damage.acceptedHit);
    CHECK(command->damage.hpOnHit == 30);
    auto heal = std::find_if(result.presentationEvents.begin(), result.presentationEvents.end(), [](const BattlePresentationEvent& event)
        {
            return event.type == BattlePresentationEventType::HealLog;
        });
    REQUIRE(heal != result.presentationEvents.end());
    CHECK(heal->sourceUnitId == 1);
    CHECK(heal->targetUnitId == 1);
    CHECK(heal->amount == 20);
    CHECK(heal->text == "命中回血");
}

TEST_CASE("BattleHitResolver_PoisonEmitsAcceptedHitCommand", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.resolvedBaseDamage = 50;
    input.attackerCombo.poisonDOTPct = 12;
    input.attackerCombo.poisonDuration = 60;

    auto result = BattleHitResolver().resolve(input);

    const auto* command = firstAcceptedHitCommand(result);
    REQUIRE(command);
    CHECK(command->damage.poisonPct == 12);
    CHECK(command->damage.poisonDurationFrames == 60);
    REQUIRE_FALSE(result.presentationEvents.empty());
    CHECK(result.presentationEvents[0].type == BattlePresentationEventType::StatusLog);
    CHECK(result.presentationEvents[0].text == "中毒（12%·60幀）");
}

TEST_CASE("BattleHitResolver_TriggeredTeamHealEmitsCommandWithoutApplyingTeamWorld", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.resolvedBaseDamage = 50;
    input.attackerCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::OnSkillTeamHeal, KysChess::Trigger::OnHit, 12, 100));
    input.percentRolls = { 0.0 };

    auto result = BattleHitResolver().resolve(input);

    auto it = std::find_if(result.commands.begin(), result.commands.end(), [](const BattleGameplayCommand& command)
        {
            return std::holds_alternative<BattleTeamHealCommand>(command);
        });
    REQUIRE(it != result.commands.end());
    const auto& command = std::get<BattleTeamHealCommand>(*it);
    CHECK(command.sourceUnitId == 1);
    CHECK(command.flatHeal == 12);
    CHECK(command.pctHeal == 0);
    CHECK(command.reason == "命中群療");
}

TEST_CASE("BattleHitResolver_ReflectedRangedProjectileChangesActualSourceAndTarget", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.attackEvent.operationType = BattleOperationType::RangedProjectile;
    input.skill.id = 101;
    input.skill.resolvedBaseDamage = 40;
    input.defenderCombo.projectileReflectPct = 100;
    input.percentRolls = { 0.0 };

    auto result = BattleHitResolver().resolve(input);

    CHECK(result.reflected);
    const auto* command = firstHpDamageCommand(result);
    REQUIRE(command);
    CHECK(command->sourceUnitId == 2);
    CHECK(command->targetUnitId == 1);
    CHECK(command->detailText == "彈反");

    auto reflectedAttribution = std::find_if(result.commands.begin(), result.commands.end(), [](const BattleGameplayCommand& command)
        {
            const auto* lastAttacker = std::get_if<BattleLastAttackerCommand>(&command);
            return lastAttacker && lastAttacker->targetUnitId == 1 && lastAttacker->attackerUnitId == 2;
        });
    REQUIRE(reflectedAttribution != result.commands.end());
}

TEST_CASE("BattleHitResolver_ExecuteTurnsFinalDamageIntoExecutedHpCommand", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.hurtType = 0;
    input.skill.resolvedBaseDamage = 20;
    input.defender.hp = 30;
    input.defender.maxHp = 100;
    input.attackerCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::Execute, KysChess::Trigger::OnHit, 50, 100));
    input.percentRolls = { 0.0 };

    auto result = BattleHitResolver().resolve(input);

    CHECK(result.executed);
    const auto* command = firstHpDamageCommand(result);
    REQUIRE(command);
    CHECK(command->sourceUnitId == 1);
    CHECK(command->targetUnitId == 2);
    CHECK(command->executed);
    CHECK(command->damage == 30);
    CHECK(command->detailText == "處決");
}

TEST_CASE("BattleHitResolver_FirstHitBlockZerosDamageAndEmitsBlockPresentation", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.hurtType = 0;
    input.skill.resolvedBaseDamage = 50;
    input.defenderCombo.blockFirstHitsRemaining = 1;

    auto result = BattleHitResolver().resolve(input);

    CHECK(result.finalHpDamage == 0);
    CHECK_FALSE(firstHpDamageCommand(result));
    CHECK(result.defenderCombo.blockFirstHitsRemaining == 0);
    auto block = std::find_if(result.presentationEvents.begin(), result.presentationEvents.end(), [](const BattlePresentationEvent& event)
        {
            return event.type == BattlePresentationEventType::StatusLog && event.text == "格擋了首轮傷害";
        });
    REQUIRE(block != result.presentationEvents.end());
}

TEST_CASE("BattleHitResolver_SkillReflectEmitsReflectedHpDamageCommand", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.hurtType = 0;
    input.skill.resolvedBaseDamage = 50;
    input.defenderCombo.skillReflectPct = 20;

    auto result = BattleHitResolver().resolve(input);

    auto reflected = std::find_if(result.commands.begin(), result.commands.end(), [](const BattleGameplayCommand& command)
        {
            const auto* hp = std::get_if<BattleHpDamageCommand>(&command);
            return hp && hp->sourceUnitId == 2 && hp->targetUnitId == 1;
        });
    REQUIRE(reflected != result.commands.end());
    const auto& command = std::get<BattleHpDamageCommand>(*reflected);
    CHECK(command.damage == 10);
    CHECK(command.detailText == "技能反彈");
}

TEST_CASE("BattleHitResolver_BleedAndMpBlockEmitAcceptedHitCommands", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.hurtType = 0;
    input.skill.resolvedBaseDamage = 50;
    input.attackerCombo.bleedChancePct = 100;
    input.attackerCombo.bleedMaxStacks = 3;
    input.attackerCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::MPBlock, KysChess::Trigger::OnHit, 9, 100));
    input.percentRolls = { 0.0, 0.0 };

    auto result = BattleHitResolver().resolve(input);

    bool sawBleed = false;
    bool sawMpBlock = false;
    for (const auto& command : result.commands)
    {
        const auto* sideEffect = std::get_if<BattleAcceptedHitSideEffectCommand>(&command);
        if (!sideEffect)
        {
            continue;
        }
        sawBleed = sawBleed || sideEffect->damage.bleedStacks == 1;
        sawMpBlock = sawMpBlock || sideEffect->damage.mpBlockFrames == 9;
    }
    CHECK(sawBleed);
    CHECK(sawMpBlock);
}

TEST_CASE("BattleHitResolver_OnHitFollowUpsEmitGameplayCommands", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.hurtType = 0;
    input.skill.resolvedBaseDamage = 50;
    input.attackerCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::CurrentHPPctBlast, KysChess::Trigger::OnHit, 15, 100));
    input.attackerCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::TeamMPRestore, KysChess::Trigger::OnHit, 8, 100));
    input.attackerCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::FlatShield, KysChess::Trigger::OnHit, 12, 100));
    input.attackerCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::SpiralBleedProjectile, KysChess::Trigger::OnHit, 2, 100));
    input.attackerCombo.triggeredEffects.back().value2 = 7;
    input.attackerCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::NearbyTrackingProjectiles, KysChess::Trigger::OnHit, 80, 100));
    input.attackerCombo.triggeredEffects.back().value2 = 45;
    input.attackerCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::UltimateExtraProjectiles, KysChess::Trigger::OnHit, 2, 100));
    input.percentRolls = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

    auto result = BattleHitResolver().resolve(input);

    bool sawCurrentHpBlast = false;
    bool sawTeamMpRestore = false;
    bool sawTeamShield = false;
    bool sawSpiral = false;
    bool sawNearby = false;
    bool sawExtra = false;
    for (const auto& command : result.commands)
    {
        if (const auto* currentHp = std::get_if<BattleCurrentHpBlastCommand>(&command))
        {
            sawCurrentHpBlast = currentHp->sourceUnitId == 1 && currentHp->damagePct == 15;
        }
        else if (const auto* teamMp = std::get_if<BattleTeamMpRestoreCommand>(&command))
        {
            sawTeamMpRestore = teamMp->sourceUnitId == 1 && teamMp->amount == 8 && teamMp->reason == "琴棋書畫";
        }
        else if (const auto* shield = std::get_if<BattleTeamShieldCommand>(&command))
        {
            sawTeamShield = shield->sourceUnitId == 1 && shield->amount == 12 && shield->refreshOnly;
        }
        else if (const auto* spiral = std::get_if<BattleSpiralBleedProjectileCommand>(&command))
        {
            sawSpiral = spiral->sourceUnitId == 1 && spiral->bleedStacks == 2 && spiral->projectileCount == 7;
        }
        else if (const auto* nearby = std::get_if<BattleNearbyTrackingProjectilesCommand>(&command))
        {
            sawNearby = nearby->prototype.sourceUnitId == 1
                && nearby->centerTargetUnitId == 2
                && nearby->rangePixels == 80
                && nearby->damagePct == 45;
        }
        else if (const auto* extra = std::get_if<BattleHitExtraProjectilesCommand>(&command))
        {
            sawExtra = extra->prototype.sourceUnitId == 1
                && extra->targetUnitId == 2
                && extra->extraCount == 2;
        }
    }

    CHECK(sawCurrentHpBlast);
    CHECK(sawTeamMpRestore);
    CHECK(sawTeamShield);
    CHECK(sawSpiral);
    CHECK(sawNearby);
    CHECK(sawExtra);
}

TEST_CASE("BattleHitResolver_SuppressedNearbyTrackingFollowUpDoesNotEmitNearbyCommand", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.hurtType = 0;
    input.skill.resolvedBaseDamage = 50;
    input.attackEvent.suppressNearbyTrackingProjectileProc = true;
    input.attackerCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::NearbyTrackingProjectiles, KysChess::Trigger::OnHit, 80, 100));
    input.percentRolls = { 0.0 };

    auto result = BattleHitResolver().resolve(input);

    auto nearby = std::find_if(result.commands.begin(), result.commands.end(), [](const BattleGameplayCommand& command)
        {
            return std::holds_alternative<BattleNearbyTrackingProjectilesCommand>(command);
        });
    CHECK(nearby == result.commands.end());
}
