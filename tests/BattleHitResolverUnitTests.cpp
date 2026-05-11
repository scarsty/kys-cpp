#include "battle/BattleCore.h"
#include "battle/BattleHitResolver.h"
#include "ChessEftIds.h"

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
    input.attacker.vitals = { 80, 100, 50, 100 };
    input.attacker.motion.position = { -1.0f, 0.0f, 0.0f };
    input.defender.id = 2;
    input.defender.vitals = { 100, 100, 20, 100 };
    input.defender.motion.position = { 0.0f, 0.0f, 0.0f };
    input.defender.motion.facing = { 1.0f, 0.0f, 0.0f };
    return input;
}

BattleRuntimeUnit runtimeUnit(int id, int team, Pointf position)
{
    BattleRuntimeUnit unit;
    unit.id = id;
    unit.team = team;
    unit.alive = true;
    unit.vitals.hp = 100;
    unit.vitals.maxHp = 100;
    unit.motion.position = position;
    return unit;
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

BattleRuntimeRandom fixedBattleRandom()
{
    return BattleRuntimeRandom(1u);
}

BattleHitResolutionResult resolveHit(const BattleHitResolutionInput& input)
{
    auto random = fixedBattleRandom();
    return KysChess::Battle::BattleHitResolver().resolve(input, random);
}

}  // namespace

TEST_CASE("BattleHitResolver_NonHitEvent_ReturnsNoGameplayCommands", "[battle][hit_resolver][unit]")
{
    BattleHitResolutionInput input;
    input.attackEvent.type = BattleAttackEventType::Moved;
    input.attacker.id = 1;
    input.defender.id = 2;

    auto result = resolveHit(input);

    CHECK(result.commands.empty());
    CHECK(result.logEvents.empty());
}

TEST_CASE("BattleHitResolver_MagicUsesResolvedBaseDamage", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.resolvedBaseDamage = 70;

    auto result = resolveHit(input);

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

    auto result = resolveHit(input);

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

    auto result = resolveHit(input);

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

    auto result = resolveHit(input);

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

TEST_CASE("BattleHitResolver_CritMarksResultAndKeepsDamageDetail", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.resolvedBaseDamage = 50;
    input.attackerCombo.critChancePct = 100;
    input.attackerCombo.critMultiplier = 200;

    auto result = resolveHit(input);

    CHECK(result.critical);
    CHECK(result.shapedHpDamage == Catch::Approx(100.0));
    CHECK(result.logEvents.empty());
    const auto* command = firstHpDamageCommand(result);
    REQUIRE(command);
    CHECK(command->critical);
    CHECK(command->detailText == "暴擊");
}

TEST_CASE("BattleHitResolver_ProjectileCritKeepsDamageDetailWithoutStatusLog", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.attackEvent.operationType = BattleOperationType::RangedProjectile;
    input.skill.id = 101;
    input.skill.resolvedBaseDamage = 50;
    input.attackerCombo.critChancePct = 100;
    input.attackerCombo.critMultiplier = 200;

    auto result = resolveHit(input);

    CHECK(result.critical);
    CHECK(result.shapedHpDamage == Catch::Approx(100.0));
    CHECK(result.logEvents.empty());
    const auto* command = firstHpDamageCommand(result);
    REQUIRE(command);
    CHECK(command->critical);
    CHECK(command->detailText == "暴擊");
}

TEST_CASE("BattleHitResolver_DamageCommandLabelsProjectileSource", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.name = "降龍";
    input.skill.resolvedBaseDamage = 70;
    input.attackEvent.operationType = BattleOperationType::RangedProjectile;
    input.attackEvent.ultimate = true;
    input.attackEvent.track = true;
    input.attackEvent.mainProjectile = false;

    auto result = resolveHit(input);

    const auto* damage = firstHpDamageCommand(result);
    REQUIRE(damage);
    CHECK(damage->detailText.find("絕招追蹤彈") != std::string::npos);
}

TEST_CASE("BattleHitResolver_HpOnHitEmitsHealPresentationAndAcceptedHitCommand", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.resolvedBaseDamage = 50;
    input.attackerCombo.hpOnHit = 30;

    auto result = resolveHit(input);

    const auto* command = firstAcceptedHitCommand(result);
    REQUIRE(command);
    CHECK(command->sourceUnitId == 1);
    CHECK(command->targetUnitId == 2);
    CHECK(command->damage.acceptedHit);
    CHECK(command->damage.hpOnHit == 30);
    auto heal = std::find_if(result.logEvents.begin(), result.logEvents.end(), [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Heal;
        });
    REQUIRE(heal != result.logEvents.end());
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

    auto result = resolveHit(input);

    const auto* command = firstAcceptedHitCommand(result);
    REQUIRE(command);
    CHECK(command->damage.poisonPct == 12);
    CHECK(command->damage.poisonDurationFrames == 60);
    REQUIRE_FALSE(result.logEvents.empty());
    CHECK(result.logEvents[0].type == BattleLogEventType::Status);
    CHECK(result.logEvents[0].text == "中毒（12%·60幀）");
}

TEST_CASE("BattleHitResolver_TriggeredTeamHealEmitsCommandWithoutApplyingTeamWorld", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.resolvedBaseDamage = 50;
    input.attackerCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::OnSkillTeamHeal, KysChess::Trigger::OnHit, 12, 100));

    auto result = resolveHit(input);

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

    auto result = resolveHit(input);

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
    input.defender.vitals.hp = 30;
    input.defender.vitals.maxHp = 100;
    input.attackerCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::Execute, KysChess::Trigger::OnHit, 50, 100));

    auto result = resolveHit(input);

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

    auto result = resolveHit(input);

    CHECK(result.finalHpDamage == 0);
    CHECK_FALSE(firstHpDamageCommand(result));
    CHECK(result.defenderCombo.blockFirstHitsRemaining == 0);
    auto block = std::find_if(result.logEvents.begin(), result.logEvents.end(), [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Status && event.text == "格擋了首輪傷害";
        });
    REQUIRE(block != result.logEvents.end());
}

TEST_CASE("BattleHitResolver_SkillReflectEmitsReflectedHpDamageCommand", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.hurtType = 0;
    input.skill.resolvedBaseDamage = 50;
    input.defenderCombo.skillReflectPct = 20;

    auto result = resolveHit(input);

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

    auto result = resolveHit(input);

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

TEST_CASE("BattleHitResolver_NearbyTrackingFollowUpEmitsGameplayCommand", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.hurtType = 0;
    input.skill.resolvedBaseDamage = 50;
    input.attackerCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::NearbyTrackingProjectiles, KysChess::Trigger::OnHit, 80, 100));
    input.attackerCombo.triggeredEffects.back().value2 = 45;

    auto result = resolveHit(input);

    bool sawNearby = false;
    for (const auto& command : result.commands)
    {
        if (const auto* nearby = std::get_if<BattleNearbyTrackingProjectilesCommand>(&command))
        {
            sawNearby = nearby->prototype.sourceUnitId == 1
                && nearby->centerTargetUnitId == 2
                && nearby->rangePixels == 80
                && nearby->damagePct == 45;
        }
    }

    CHECK(sawNearby);
}

TEST_CASE("BattleHitResolver_OnHitDoesNotEmitCastScopedFollowUps", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.attackEvent.operationType = BattleOperationType::RangedProjectile;
    input.attackEvent.velocity = { 4.0f, 0.0f, 0.0f };
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

    auto result = resolveHit(input);

    for (const auto& command : result.commands)
    {
        CHECK_FALSE(std::holds_alternative<BattleCurrentHpBlastCommand>(command));
        CHECK_FALSE(std::holds_alternative<BattleTeamMpRestoreCommand>(command));
        CHECK_FALSE(std::holds_alternative<BattleTeamShieldCommand>(command));
        CHECK_FALSE(std::holds_alternative<BattleSpiralBleedProjectileCommand>(command));
    }
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

    auto result = resolveHit(input);

    auto nearby = std::find_if(result.commands.begin(), result.commands.end(), [](const BattleGameplayCommand& command)
        {
            return std::holds_alternative<BattleNearbyTrackingProjectilesCommand>(command);
    });
    CHECK(nearby == result.commands.end());
}

TEST_CASE("BattleProjectileFollowUpResolver_CurrentHpBlastDoesNotTriggerDefenseEffects", "[battle][hit_resolver][unit]")
{
    BattleProjectileFollowUpContext context;
    BattleUnitStore units;
    units.units = {
        runtimeUnit(0, 0, { 0.0f, 0.0f, 0.0f }),
        runtimeUnit(1, 1, { 40.0f, 0.0f, 0.0f }),
    };

    std::vector<BattleGameplayCommand> commands;
    commands.push_back(BattleCurrentHpBlastCommand{ 0, 15, "當前生命傷害" });

    auto expanded = expandBattleProjectileFollowUpCommands(commands, context, units);

    REQUIRE(expanded.commands.size() == 1);
    const auto* hp = std::get_if<BattleHpDamageCommand>(&expanded.commands[0]);
    REQUIRE(hp);
    CHECK_FALSE(hp->triggersDefenseEffects);
    CHECK(hp->detailText == "當前生命傷害");
}

TEST_CASE("BattleHitResolver_SkillReflectDamageDoesNotTriggerDefenseEffects", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.resolvedBaseDamage = 80;
    input.defenderCombo.skillReflectPct = 50;

    auto result = resolveHit(input);

    auto reflected = std::find_if(result.commands.begin(), result.commands.end(), [](const BattleGameplayCommand& command)
        {
            const auto* hp = std::get_if<BattleHpDamageCommand>(&command);
            return hp && hp->detailText == "技能反彈";
        });
    REQUIRE(reflected != result.commands.end());
    const auto& hp = std::get<BattleHpDamageCommand>(*reflected);
    CHECK_FALSE(hp.triggersDefenseEffects);
}

TEST_CASE("BattleProjectileFollowUpResolver_ExpandsNearbyTrackingIntoSpawnCommands", "[battle][hit_resolver][unit]")
{
    BattleProjectileFollowUpContext context;
    BattleUnitStore units;
    units.units = {
        runtimeUnit(0, 0, { 0.0f, 0.0f, 0.0f }),
        runtimeUnit(1, 1, { 40.0f, 0.0f, 0.0f }),
        runtimeUnit(2, 1, { 80.0f, 0.0f, 0.0f }),
    };
    units.units[1].grid = { 1, 0 };
    units.units[2].grid = { 2, 0 };
    context.projectileSpeed = 10.0;

    BattleAttackEvent prototype;
    prototype.type = BattleAttackEventType::Hit;
    prototype.attackId = 10;
    prototype.sourceUnitId = 0;
    prototype.unitId = 1;
    prototype.skillId = 101;
    prototype.visualEffectId = 44;
    prototype.position = { 0, 0, 0 };
    prototype.velocity = { 5, 0, 0 };
    prototype.totalFrame = 30;
    prototype.operationType = BattleOperationType::RangedProjectile;

    std::vector<BattleGameplayCommand> commands;
    commands.push_back(BattleNearbyTrackingProjectilesCommand{
        prototype,
        2,
        100,
        40,
    });

    auto expanded = expandBattleProjectileFollowUpCommands(commands, context, units);

    REQUIRE(expanded.commands.size() == 2);
    const auto* first = std::get_if<BattleProjectileSpawnCommand>(&expanded.commands[0]);
    const auto* second = std::get_if<BattleProjectileSpawnCommand>(&expanded.commands[1]);
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first->request.initial.preferredTargetUnitId == 2);
    CHECK(second->request.initial.preferredTargetUnitId == 1);
    CHECK(first->request.initial.suppressNearbyTrackingProjectileProc);
    CHECK_FALSE(first->request.initial.mainProjectile);
    CHECK(first->request.initial.strengthMultiplier == Catch::Approx(0.4f));
}

TEST_CASE("BattleProjectileFollowUpResolver_NearbyTrackingUsesCommandProjectileSpeed", "[battle][hit_resolver][unit]")
{
    BattleProjectileFollowUpContext context;
    BattleUnitStore units;
    units.units = {
        runtimeUnit(0, 0, { 0.0f, 0.0f, 0.0f }),
        runtimeUnit(1, 1, { 80.0f, 0.0f, 0.0f }),
    };
    context.projectileSpeed = 10.0;

    BattleAttackEvent prototype;
    prototype.sourceUnitId = 0;
    prototype.unitId = 1;
    prototype.skillId = 101;
    prototype.visualEffectId = 44;
    prototype.position = { 0, 0, 0 };
    prototype.velocity = { 20, 0, 0 };

    std::vector<BattleGameplayCommand> commands;
    commands.push_back(BattleNearbyTrackingProjectilesCommand{
        prototype,
        1,
        100,
        40,
        20.0,
    });

    auto expanded = expandBattleProjectileFollowUpCommands(commands, context, units);

    REQUIRE(expanded.commands.size() == 1);
    const auto* spawn = std::get_if<BattleProjectileSpawnCommand>(&expanded.commands[0]);
    REQUIRE(spawn);
    CHECK(spawn->request.initial.velocity.x == Catch::Approx(20.0f));
    CHECK(spawn->request.initial.velocity.y == Catch::Approx(0.0f));
    CHECK(spawn->request.initial.velocity.z == Catch::Approx(0.0f));
}

TEST_CASE("BattleProjectileFollowUpResolver_SpiralBleedUsesCommandProjectileSpeed", "[battle][hit_resolver][unit]")
{
    BattleProjectileFollowUpContext context;
    BattleUnitStore units;
    units.units = {
        runtimeUnit(0, 0, { 0.0f, 0.0f, 0.0f }),
    };
    context.projectileSpeed = 10.0;

    std::vector<BattleGameplayCommand> commands;
    commands.push_back(BattleSpiralBleedProjectileCommand{
        0,
        3,
        2,
        20.0,
    });

    auto expanded = expandBattleProjectileFollowUpCommands(commands, context, units);

    REQUIRE(expanded.commands.size() == 2);
    const auto* spawn = std::get_if<BattleProjectileSpawnCommand>(&expanded.commands[0]);
    REQUIRE(spawn);
    CHECK(spawn->request.spiralRadiusGrowth == Catch::Approx(18.0f));
}

TEST_CASE("BattleHitResolver_MpDamageSkillEmitsMpDamageCommand", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.hurtType = 1;
    input.skill.resolvedBaseDamage = 50;
    input.randomDamageVariance = -5;

    auto result = resolveHit(input);

    auto mpDamage = std::find_if(result.commands.begin(), result.commands.end(), [](const BattleGameplayCommand& command)
        {
            return std::holds_alternative<BattleMpDamageCommand>(command);
        });
    REQUIRE(mpDamage != result.commands.end());
    const auto& command = std::get<BattleMpDamageCommand>(*mpDamage);
    CHECK(command.sourceUnitId == 1);
    CHECK(command.targetUnitId == 2);
    CHECK(command.damage.mpDamage == 45);
    CHECK(command.damage.mpOnHit == 36);
    CHECK(result.finalHpDamage == 0);
    CHECK(result.finalMpDamage == 45);
}

TEST_CASE("BattleHitResolver_ShieldBreakEmitsShieldBreakCommands", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.hurtType = 0;
    input.skill.resolvedBaseDamage = 50;
    input.defenderCombo.shield = 30;
    input.defenderCombo.shieldPctMaxHP = 20;
    input.defenderCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::ShieldExplosion, KysChess::Trigger::OnShieldBreak, 50, 100));
    input.defenderCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::AutoUltimate, KysChess::Trigger::OnShieldBreak, 1, 100));
    input.defenderCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::TempFlatATK, KysChess::Trigger::OnShieldBreak, 14, 100, 45));
    input.defenderCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::MPRestore, KysChess::Trigger::OnShieldBreak, 25, 100));

    auto result = resolveHit(input);

    bool sawExplosion = false;
    bool sawUltimate = false;
    bool sawAttackBuff = false;
    bool sawMpRestore = false;
    for (const auto& command : result.commands)
    {
        if (const auto* explosion = std::get_if<BattleShieldExplosionCommand>(&command))
        {
            sawExplosion = explosion->sourceUnitId == 2
                && explosion->areaSize == 5
                && explosion->effectId == KysChess::EFT_SHIELD_BLAST
                && explosion->damage == 10;
        }
        else if (const auto* ultimate = std::get_if<BattleAutoUltimateCommand>(&command))
        {
            sawUltimate = ultimate->unitId == 2 && !ultimate->consumeMp;
        }
        else if (const auto* attackBuff = std::get_if<BattleTempAttackBuffCommand>(&command))
        {
            sawAttackBuff = attackBuff->unitId == 2
                && attackBuff->attackBonus == 14
                && attackBuff->durationFrames == 45;
        }
        else if (const auto* mpRestore = std::get_if<BattleMpRestoreCommand>(&command))
        {
            sawMpRestore = mpRestore->unitId == 2 && mpRestore->amount == 25;
        }
    }

    CHECK(sawExplosion);
    CHECK(sawUltimate);
    CHECK(sawAttackBuff);
    CHECK(sawMpRestore);
}

TEST_CASE("BattleHitResolver_ScriptedImpactEmitsStatusAndDamageCommands", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = -1;
    input.attackEvent.operationType = BattleOperationType::None;
    input.attackEvent.scriptedStunFrames = 12;
    input.attackEvent.scriptedBleedStacks = 2;
    input.attackEvent.scriptedDamage = 35;
    input.sharedBleedMaxStacks = 4;

    auto result = resolveHit(input);

    bool sawStatus = false;
    bool sawDamage = false;
    bool sawStunLog = false;
    bool sawBleedLog = false;
    for (const auto& command : result.commands)
    {
        if (const auto* sideEffect = std::get_if<BattleAcceptedHitSideEffectCommand>(&command))
        {
            sawStatus = sideEffect->sourceUnitId == 1
                && sideEffect->targetUnitId == 2
                && sideEffect->damage.frozenFrames == 12
                && sideEffect->damage.bleedStacks == 2
                && sideEffect->damage.bleedMaxStacks == 4;
        }
        else if (const auto* hp = std::get_if<BattleHpDamageCommand>(&command))
        {
            sawDamage = hp->sourceUnitId == 1
                && hp->targetUnitId == 2
                && hp->damage == 35
                && hp->detailText == "特效傷害";
        }
    }
    for (const auto& event : result.logEvents)
    {
        sawStunLog = sawStunLog || (event.type == BattleLogEventType::Status && event.text == "眩暈（12幀）");
        sawBleedLog = sawBleedLog || (event.type == BattleLogEventType::Status && event.text == "螺旋流血彈流血2層");
    }

    CHECK(sawStatus);
    CHECK(sawDamage);
    CHECK(sawStunLog);
    CHECK(sawBleedLog);
}
