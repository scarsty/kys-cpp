#include "battle/BattleDamageApplicationSystem.h"
#include "battle/BattleCore.h"
#include "ChessEftIds.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <variant>

using namespace KysChess::Battle;
using KysChess::EffectType;
using KysChess::Trigger;

namespace
{

BattleDamageTransactionInput damageInput(int attackerUnitId, int defenderUnitId, int damage)
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = attackerUnitId;
    input.request.defenderUnitId = defenderUnitId;
    input.request.baseDamage = damage;
    input.request.preResolvedDamage = true;
    input.attacker.id = attackerUnitId;
    input.attacker.alive = true;
    input.attacker.vitals.hp = 40;
    input.attacker.vitals.maxHp = 100;
    input.attacker.attack = 12;
    input.defender.id = defenderUnitId;
    input.defender.alive = true;
    input.defender.vitals.hp = 10;
    input.defender.vitals.maxHp = 100;
    input.defenderStatus.id = defenderUnitId;
    input.defenderStatus.alive = true;
    input.defenderStatus.hp = 10;
    input.defenderStatus.maxHp = 100;
    return input;
}

KysChess::AppliedEffectInstance triggeredEffect(KysChess::EffectType type,
                                                KysChess::Trigger trigger,
                                                int value,
                                                int triggerValue = 0,
                                                int duration = 0)
{
    KysChess::AppliedEffectInstance effect;
    effect.type = type;
    effect.trigger = trigger;
    effect.value = value;
    effect.triggerValue = triggerValue;
    effect.duration = duration;
    return effect;
}

BattleRuntimeUnit runtimeUnit(int id, int team, int hp, int maxHp, int attack)
{
    BattleRuntimeUnit unit;
    unit.id = id;
    unit.team = team;
    unit.alive = hp > 0;
    unit.vitals.hp = hp;
    unit.vitals.maxHp = maxHp;
    unit.stats.attack = attack;
    return unit;
}

struct BattleDamageApplicationTestFrame
{
    BattleDamageApplicationInput input;
    BattleUnitStore runtimeUnits;
    std::map<int, KysChess::RoleComboState> comboUnits;
    std::vector<BattleStatusRuntimeUnit> statusUnits;
    std::vector<BattleDamageRuntimeUnit> damageUnitExtras;
    BattleDeathEffectStore deathEffects;
    std::vector<BattlePendingDamageIntent> pendingDamage;
    std::map<int, BattleDamageApplicationUnitEffects> unitEffects;
    BattleProjectileFollowUpContext projectileFollowUps;
    BattleRuntimeRandom random{ 1u };

    BattleDamageApplicationTestFrame()
    {
        input.frame = 77;
        BattleRuntimeUnit attacker;
        attacker.id = 0;
        attacker.team = 0;
        attacker.alive = true;
        attacker.vitals.hp = 40;
        attacker.vitals.maxHp = 100;
        attacker.motion.position = { 0.0f, 0.0f, 0.0f };
        attacker.grid = { 0, 0 };
        BattleRuntimeUnit defender;
        defender.id = 1;
        defender.team = 1;
        defender.alive = true;
        defender.vitals.hp = 10;
        defender.vitals.maxHp = 100;
        defender.motion.position = { 36.0f, 0.0f, 0.0f };
        defender.grid = { 1, 0 };
        runtimeUnits.units = { attacker, defender };
        BattleDeathEffectExtras attackerEffects;
        attackerEffects.id = 0;
        BattleDeathEffectExtras defenderEffects;
        defenderEffects.id = 1;
        deathEffects.units = { attackerEffects, defenderEffects };
        BattleStatusRuntimeUnit attackerStatus;
        attackerStatus.id = 0;
        BattleStatusRuntimeUnit defenderStatus;
        defenderStatus.id = 1;
        statusUnits = { attackerStatus, defenderStatus };
        BattleDamageRuntimeUnit attackerDamage;
        attackerDamage.id = 0;
        BattleDamageRuntimeUnit defenderDamage;
        defenderDamage.id = 1;
        damageUnitExtras = { attackerDamage, defenderDamage };
        input.units = &runtimeUnits;
        comboUnits.emplace(0, KysChess::RoleComboState{});
        comboUnits.emplace(1, KysChess::RoleComboState{});
        input.comboUnits = &comboUnits;
        input.statusUnits = &statusUnits;
        input.damageUnitExtras = &damageUnitExtras;
        input.deathEffects = &deathEffects;
        input.pendingDamage = &pendingDamage;
        input.unitEffects = &unitEffects;
        input.projectileFollowUps = &projectileFollowUps;
        input.random = &random;
        projectileFollowUps.projectileSpeed = 12.0;
    }
};

BattleDamageApplicationTestFrame applicationInput()
{
    return BattleDamageApplicationTestFrame{};
}

const BattleDamageLifecycleEvent* findLifecycleEvent(
    const BattleDamageApplicationResult& result,
    BattleDamageLifecycleEventType type)
{
    for (const auto& event : result.lifecycleEvents)
    {
        if (event.type == type)
        {
            return &event;
        }
    }
    return nullptr;
}

template <typename T>
T* findById(std::vector<T>& items, int id)
{
    for (auto& item : items)
    {
        if (item.id == id)
        {
            return &item;
        }
    }
    return nullptr;
}

void seedRuntimeDamageUnit(BattleDamageApplicationTestFrame& frame, const BattleDamageUnitState& unit)
{
    if (unit.id < 0)
    {
        return;
    }

    frame.runtimeUnits.writeDamageUnit(unit);
    auto* runtimeDamage = findById(frame.damageUnitExtras, unit.id);
    if (!runtimeDamage)
    {
        BattleDamageRuntimeUnit added;
        added.id = unit.id;
        frame.damageUnitExtras.push_back(added);
        runtimeDamage = &frame.damageUnitExtras.back();
    }
    writeBattleDamageRuntimeUnit(*runtimeDamage, unit);
}

void seedRuntimeStatusUnit(BattleDamageApplicationTestFrame& frame, const BattleStatusUnitState& unit)
{
    auto* runtimeStatus = findById(frame.statusUnits, unit.id);
    if (!runtimeStatus)
    {
        BattleStatusRuntimeUnit added;
        added.id = unit.id;
        frame.statusUnits.push_back(added);
        runtimeStatus = &frame.statusUnits.back();
    }
    writeBattleStatusRuntimeUnit(*runtimeStatus, unit);
}

void seedComboModifierState(
    BattleDamageApplicationTestFrame& frame,
    int unitId,
    const BattleDamageModifierState& modifier)
{
    if (unitId < 0)
    {
        return;
    }

    auto& combo = frame.comboUnits[unitId];
    combo.flatDmgIncrease = modifier.flatDamageIncrease;
    combo.skillDmgPct = modifier.skillDamagePct;
    combo.poisonDmgAmpPct = modifier.poisonDamageAmpPct;
    combo.flatDmgReduction = modifier.flatDamageReduction;
    combo.dmgReductionPct = modifier.damageReductionPct;
    combo.maxHitPctCurrentHP = modifier.maxHitPctMaxHp;
    combo.poisonTimer = modifier.poisonTimer;
    combo.dmgReduceDebuffs.clear();
    for (const auto& debuff : modifier.outgoingDamageReduceDebuffs)
    {
        combo.dmgReduceDebuffs.push_back({ debuff.remainingFrames, debuff.pct });
    }
}

void queuePendingDamage(
    BattleDamageApplicationTestFrame& frame,
    BattleDamageTransactionInput transaction,
    BattleDamagePresentationInput presentation = {})
{
    seedRuntimeDamageUnit(frame, transaction.attacker);
    seedRuntimeDamageUnit(frame, transaction.defender);
    seedRuntimeStatusUnit(frame, transaction.defenderStatus);
    seedComboModifierState(frame, transaction.attacker.id, transaction.attackerModifiers);
    seedComboModifierState(frame, transaction.defender.id, transaction.defenderModifiers);
    frame.pendingDamage.push_back({ transaction.request, std::move(presentation) });
}

}  // namespace

TEST_CASE("BattleDamageApplication_FatalDamageEmitsDeathAndKillRewardEvents", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    auto damage = damageInput(0, 1, 20);
    damage.attacker.killHealPct = 25;
    damage.attacker.bloodlustAttackPerKill = 7;
    queuePendingDamage(frame, damage);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 1);
    CHECK(result.transactions[0].killed);
    REQUIRE(findLifecycleEvent(result, BattleDamageLifecycleEventType::UnitDied));
    CHECK(findLifecycleEvent(result, BattleDamageLifecycleEventType::UnitDied)->targetUnitId == 1);
    REQUIRE(findLifecycleEvent(result, BattleDamageLifecycleEventType::KillRecorded));
    CHECK(findLifecycleEvent(result, BattleDamageLifecycleEventType::KillRecorded)->sourceUnitId == 0);
    CHECK(result.transactions[0].attacker.vitals.hp == 65);
    CHECK(result.transactions[0].attacker.attack == 19);
    REQUIRE(result.gameplayEvents.size() >= 1);
    CHECK(result.gameplayEvents[0].type == BattleGameplayEventType::UnitDied);
    CHECK(result.gameplayEvents[0].frame == 77);
    CHECK(result.gameplayEvents[0].sourceUnitId == 0);
    CHECK(result.gameplayEvents[0].targetUnitId == 1);
}

TEST_CASE("BattleDamageApplication_FatalDamageLogsDamageBeforeDeath", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    BattleDamagePresentationInput presentation;
    presentation.skillName = "六脈神劍";
    queuePendingDamage(frame, damageInput(0, 1, 20), presentation);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 1);
    REQUIRE(result.transactions[0].killed);
    REQUIRE(result.logEvents.size() >= 2);
    CHECK(result.logEvents[0].type == BattleLogEventType::Damage);
    CHECK(result.logEvents[0].sourceUnitId == 0);
    CHECK(result.logEvents[0].targetUnitId == 1);
    CHECK(result.logEvents[0].amount == 10);
    CHECK(result.logEvents[0].skillName == "六脈神劍");
    CHECK(result.logEvents[1].type == BattleLogEventType::UnitDied);
    CHECK(result.logEvents[1].sourceUnitId == 0);
    CHECK(result.logEvents[1].targetUnitId == 1);
}

TEST_CASE("BattleDamageApplication_ResolvesPendingDamageByMagnitudeWhenSortingEnabled", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    input.sortPendingDamageByDefenderMagnitude = true;
    input.units->units.push_back(runtimeUnit(2, 0, 80, 100, 0));

    auto first = damageInput(0, 1, 3);
    auto second = damageInput(2, 1, 4);
    second.attacker.id = 2;
    second.attacker.vitals.hp = 80;
    second.attacker.vitals.maxHp = 100;
    queuePendingDamage(frame, first);
    queuePendingDamage(frame, second);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 2);
    CHECK(result.transactions[0].attacker.id == 2);
    CHECK(result.transactions[0].finalHpDamage == 4);
    CHECK(result.transactions[1].attacker.id == 0);
    CHECK(result.transactions[1].finalHpDamage == 3);
    CHECK(result.transactions.back().defender.vitals.hp == 3);
}

TEST_CASE("BattleDamageApplication_PreservesPerHitPresentationMetadata", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    input.sortPendingDamageByDefenderMagnitude = true;
    input.units->units.push_back(runtimeUnit(2, 0, 80, 100, 0));

    auto first = damageInput(0, 1, 3);
    auto second = damageInput(2, 1, 4);
    second.attacker.id = 2;

    BattleDamagePresentationInput firstPresentation;
    firstPresentation.enabled = true;
    firstPresentation.skillName = "先手";
    firstPresentation.detailText = "第一段";
    firstPresentation.normalDamageColor = { 10, 20, 30, 255 };
    firstPresentation.normalDamageTextSize = 22;

    BattleDamagePresentationInput secondPresentation;
    secondPresentation.enabled = true;
    secondPresentation.skillName = "終段";
    secondPresentation.detailText = "第二段";
    secondPresentation.critical = true;
    secondPresentation.emphasizedDamageColor = { 40, 50, 60, 255 };
    secondPresentation.emphasizedDamageTextSize = 33;
    queuePendingDamage(frame, first, firstPresentation);
    queuePendingDamage(frame, second, secondPresentation);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 2);
    REQUIRE(result.visualEvents.size() == 2);
    CHECK(result.visualEvents[0].type == BattleVisualEventType::DamageNumber);
    CHECK(result.visualEvents[0].targetUnitId == 1);
    CHECK(result.visualEvents[0].amount == 4);
    CHECK(result.visualEvents[0].textSize == 33);
    CHECK(result.visualEvents[0].color.r == 40);
    CHECK(result.visualEvents[1].amount == 3);
    REQUIRE(result.logEvents.size() == 2);
    CHECK(result.logEvents[0].type == BattleLogEventType::Damage);
    CHECK(result.logEvents[0].sourceUnitId == 2);
    CHECK(result.logEvents[0].targetUnitId == 1);
    CHECK(result.logEvents[0].amount == 4);
    CHECK(result.logEvents[0].skillName == "終段");
    CHECK(result.logEvents[0].detailText == "第二段");
    CHECK(result.logEvents[1].sourceUnitId == 0);
    CHECK(result.logEvents[1].amount == 3);
    CHECK(result.logEvents[1].skillName == "先手");
    CHECK(result.logEvents[1].detailText == "第一段");
}

TEST_CASE("BattleDamageApplication_AppliesHurtInvincibilityBetweenSameFrameHits", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    input.sortPendingDamageByDefenderMagnitude = true;

    auto first = damageInput(0, 1, 3);
    first.defender.hurtInvincFrames = 5;
    first.request.triggersDefenseEffects = true;

    auto second = damageInput(0, 1, 4);
    second.defender.hurtInvincFrames = 5;
    second.request.triggersDefenseEffects = true;

    queuePendingDamage(frame, first);
    queuePendingDamage(frame, second);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 2);
    CHECK(result.transactions[0].finalHpDamage == 4);
    CHECK(result.transactions[0].hurtInvincGranted);
    CHECK(result.transactions[1].finalHpDamage == 0);
    CHECK(result.transactions[1].blockedByInvincible);
    CHECK(input.units->requireUnit(1).vitals.hp == 6);
    CHECK(input.units->requireUnit(1).invincible == 5);
}

TEST_CASE("BattleDamageApplication_AppliesSingleHitCapPerPendingHit", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    input.sortPendingDamageByDefenderMagnitude = true;

    auto first = damageInput(0, 1, 60);
    first.request.preResolvedDamage = false;
    first.defenderModifiers.maxHitPctMaxHp = 25;
    first.defender.vitals.hp = 100;
    first.defender.vitals.maxHp = 100;
    first.defenderStatus.hp = 100;
    first.defenderStatus.maxHp = 100;

    auto second = damageInput(0, 1, 60);
    second.request.preResolvedDamage = false;
    second.defenderModifiers.maxHitPctMaxHp = 25;
    second.defender.vitals.hp = 100;
    second.defender.vitals.maxHp = 100;
    second.defenderStatus.hp = 100;
    second.defenderStatus.maxHp = 100;

    queuePendingDamage(frame, first);
    queuePendingDamage(frame, second);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 2);
    CHECK(result.transactions[0].finalHpDamage == 25);
    CHECK(result.transactions[1].finalHpDamage == 25);
    CHECK(input.units->requireUnit(1).vitals.hp == 50);
}

TEST_CASE("BattleDamageApplication_UsesStatusDamageReduceDebuffsWithoutComboMirror", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;

    auto damage = damageInput(0, 1, 10);
    damage.request.preResolvedDamage = false;
    damage.defender.vitals.hp = 100;
    damage.defender.vitals.maxHp = 100;
    damage.defenderStatus.hp = 100;
    damage.defenderStatus.maxHp = 100;
    queuePendingDamage(frame, damage);

    auto* attackerStatus = findById(frame.statusUnits, 0);
    REQUIRE(attackerStatus);
    attackerStatus->effects.damageReduceDebuffs.push_back({ 3, 50 });
    frame.comboUnits.at(0).dmgReduceDebuffs.clear();

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 1);
    CHECK(result.transactions[0].finalHpDamage == 5);
    CHECK(frame.comboUnits.at(0).dmgReduceDebuffs.empty());
}

TEST_CASE("BattleDamageApplication_OrdersDamageNumbersByLargestHitPerDefender", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    input.sortPendingDamageByDefenderMagnitude = true;

    BattleDamagePresentationInput firstPresentation;
    firstPresentation.enabled = true;
    firstPresentation.normalDamageColor = { 10, 20, 30, 255 };
    firstPresentation.normalDamageTextSize = 22;

    BattleDamagePresentationInput secondPresentation = firstPresentation;

    queuePendingDamage(frame, damageInput(0, 1, 3), firstPresentation);
    queuePendingDamage(frame, damageInput(0, 1, 7), secondPresentation);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 2);
    CHECK(result.transactions[0].finalHpDamage == 7);
    CHECK(result.transactions[1].finalHpDamage == 3);
    REQUIRE(result.visualEvents.size() == 2);
    CHECK(result.visualEvents[0].amount == 7);
    CHECK(result.visualEvents[1].amount == 3);
}

TEST_CASE("BattleDamageApplication_AccumulatesDamageTakenMpGainAcrossSameFrameHits", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;

    auto first = damageInput(0, 1, 20);
    first.defender.vitals.hp = 100;
    first.defender.vitals.maxHp = 100;
    first.defender.vitals.mp = 5;
    first.defender.vitals.maxMp = 100;
    first.defender.mpRecoveryBonusPct = 50;
    first.defenderStatus.hp = 100;
    first.defenderStatus.maxHp = 100;

    auto second = damageInput(0, 1, 20);
    second.defender.vitals.hp = 100;
    second.defender.vitals.maxHp = 100;
    second.defender.vitals.mp = 5;
    second.defender.vitals.maxMp = 100;
    second.defender.mpRecoveryBonusPct = 50;
    second.defenderStatus.hp = 100;
    second.defenderStatus.maxHp = 100;

    queuePendingDamage(frame, first);
    queuePendingDamage(frame, second);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 2);
    CHECK(result.transactions[0].defender.vitals.mp == 27);
    CHECK(result.transactions[0].defenderDelta.mpDelta == 22);
    CHECK(result.transactions[1].defender.vitals.mp == 49);
    CHECK(result.transactions[1].defenderDelta.mpDelta == 22);
    CHECK(input.units->requireUnit(1).vitals.mp == 49);
}

TEST_CASE("BattleDamageApplication_PreviewDefenderPendingHpDamageUsesResolvedPendingDamage", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;

    auto blocked = damageInput(0, 1, 20);
    blocked.defender.blockFirstHitsRemaining = 1;
    queuePendingDamage(frame, blocked);

    CHECK(BattleDamageApplicationSystem().previewDefenderPendingHpDamage(input, 1) == 0);
}

TEST_CASE("BattleDamageApplication_UsesDamageRuntimeBlockFirstHitWithoutComboMirror", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;

    auto damage = damageInput(0, 1, 9);
    queuePendingDamage(frame, damage);
    auto* defenderExtras = findById(frame.damageUnitExtras, 1);
    REQUIRE(defenderExtras);
    defenderExtras->blockFirstHitsRemaining = 1;
    frame.comboUnits.at(1).blockFirstHitsRemaining = 0;

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 1);
    CHECK(result.transactions[0].blockedByFirstHit);
    CHECK(result.transactions[0].finalHpDamage == 0);
    REQUIRE(defenderExtras);
    CHECK(defenderExtras->blockFirstHitsRemaining == 0);
    CHECK(frame.comboUnits.at(1).blockFirstHitsRemaining == 0);
}

TEST_CASE("BattleDamageApplication_ShieldBreakEmitsShieldBreakCommands", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    auto damage = damageInput(0, 1, 50);
    damage.defender.shield = 30;
    damage.defender.vitals.mp = 10;
    damage.defender.vitals.maxMp = 100;
    queuePendingDamage(frame, damage);
    auto& defenderCombo = frame.comboUnits.at(1);
    defenderCombo.shield = 0;
    defenderCombo.shieldPctMaxHP = 20;
    defenderCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::ShieldExplosion, KysChess::Trigger::OnShieldBreak, 50, 100));
    defenderCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::AutoUltimate, KysChess::Trigger::OnShieldBreak, 1, 100));
    defenderCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::TempFlatATK, KysChess::Trigger::OnShieldBreak, 14, 100, 45));
    defenderCombo.triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::MPRestore, KysChess::Trigger::OnShieldBreak, 25, 100));

    auto result = BattleDamageApplicationSystem().apply(input);

    bool sawExplosion = false;
    bool sawUltimate = false;
    bool sawAttackBuff = false;
    bool sawMpRestore = false;
    sawExplosion = std::ranges::any_of(result.logEvents, [](const BattleLogEvent& event)
    {
        return event.type == BattleLogEventType::Status
            && event.sourceUnitId == 1
            && event.text == "護盾爆炸（10傷害）";
    });
    for (const auto& command : result.commands)
    {
        if (const auto* ultimate = std::get_if<BattleAutoUltimateCommand>(&command))
        {
            sawUltimate = ultimate->unitId == 1 && !ultimate->consumeMp;
        }
        else if (const auto* attackBuff = std::get_if<BattleTempAttackBuffCommand>(&command))
        {
            sawAttackBuff = attackBuff->unitId == 1
                && attackBuff->attackBonus == 14
                && attackBuff->durationFrames == 45;
        }
        else if (const auto* mpRestore = std::get_if<BattleMpRestoreCommand>(&command))
        {
            sawMpRestore = mpRestore->unitId == 1 && mpRestore->amount == 25;
        }
    }

    CHECK(sawExplosion);
    CHECK(sawUltimate);
    CHECK(sawAttackBuff);
    CHECK(sawMpRestore);
}

TEST_CASE("BattleDamageApplication_DeathPreventionLeavesUnitAliveAndEmitsLog", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    auto damage = damageInput(0, 1, 20);
    damage.defender.deathPrevention = true;
    damage.defender.deathPreventionFrames = 30;
    queuePendingDamage(frame, damage);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 1);
    CHECK_FALSE(result.transactions[0].killed);
    CHECK(result.transactions[0].defender.alive);
    CHECK(result.transactions[0].defender.vitals.hp == 1);
    auto preventionLog = std::find_if(result.logEvents.begin(), result.logEvents.end(), [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Status
                && event.sourceUnitId == 1
                && event.targetUnitId == 1
                && event.amount == 30
                && event.text == "死亡庇護（30幀）";
        });
    CHECK(preventionLog != result.logEvents.end());
}

TEST_CASE("BattleDamageApplication_DeathAoeBecomesProjectileCommand", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    queuePendingDamage(frame, damageInput(0, 1, 20));
    frame.unitEffects[1].deathAoePct = 40;
    frame.unitEffects[1].deathAoeStunFrames = 5;
    frame.unitEffects[1].deathAoeMaxTargets = 3;

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.commands.size() == 1);
    const auto* command = std::get_if<BattleProjectileSpawnCommand>(&result.commands[0]);
    REQUIRE(command);
    CHECK(command->request.initial.attackerUnitId == 1);
    CHECK(command->request.initial.preferredTargetUnitId == 0);
    CHECK(command->request.initial.scriptedDamage == 40);
    CHECK(command->request.initial.scriptedStunFrames == 5);
    CHECK(command->request.initial.track);
}

TEST_CASE("BattleDamageApplication_StatusAppliedProducesStatusLog", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    auto damage = damageInput(0, 1, 5);
    damage.request.bleedStacks = 2;
    damage.request.bleedMaxStacks = 4;
    queuePendingDamage(frame, damage);

    auto result = BattleDamageApplicationSystem().apply(input);

    auto bleedLog = std::find_if(result.logEvents.begin(), result.logEvents.end(), [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Status
                && event.sourceUnitId == 0
                && event.targetUnitId == 1
                && event.amount == 2
                && event.text == "流血（2/4層）";
        });
    CHECK(bleedLog != result.logEvents.end());
}

TEST_CASE("BattleDamageApplication_StatusTickPresentationProducesColoredDamageNumber", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;

    BattleDamagePresentationInput presentation;
    presentation.enabled = true;
    presentation.detailText = "流血";
    presentation.normalDamageColor = { 190, 120, 60, 255 };
    presentation.normalDamageTextSize = 22;
    queuePendingDamage(frame, damageInput(0, 1, 6), presentation);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.visualEvents.size() == 1);
    CHECK(result.visualEvents[0].type == BattleVisualEventType::DamageNumber);
    CHECK(result.visualEvents[0].targetUnitId == 1);
    CHECK(result.visualEvents[0].amount == 6);
    CHECK(result.visualEvents[0].textSize == 22);
    CHECK(result.visualEvents[0].color.r == 190);
    CHECK(result.visualEvents[0].color.g == 120);
    CHECK(result.visualEvents[0].color.b == 60);
}

TEST_CASE("BattleDamageApplication_AllyDeathEffectsBecomeExplicitCommands", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    queuePendingDamage(frame, damageInput(0, 1, 20));

    BattleRuntimeUnit allyUnit;
    allyUnit.id = 2;
    allyUnit.team = 1;
    allyUnit.alive = true;
    allyUnit.vitals.hp = 50;
    allyUnit.vitals.maxHp = 100;
    allyUnit.stats.attack = 10;
    allyUnit.stats.defence = 8;
    input.units->units.push_back(allyUnit);

    BattleDeathEffectExtras dead;
    dead.id = 1;
    dead.comboIds = { 9 };
    dead.appliedEffects.push_back({ EffectType::DeathMedical, 20, 0, "", Trigger::Always, 0, 0, 0, 9 });

    BattleDeathEffectExtras ally;
    ally.id = 2;
    ally.shieldPctMaxHp = 30;
    ally.comboIds = { 9 };
    ally.appliedEffects.push_back({ EffectType::AllyDeathStatBoost, 4, 0, "", Trigger::Always, 0, 0, 0, 9 });
    ally.appliedEffects.push_back({ EffectType::ShieldOnAllyDeath, 1, 0, "", Trigger::Always, 0, 0, 0, 9 });

    input.deathEffects->units = { dead, ally };
    input.deathEffects->regularSynergyComboIds.insert(9);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.commands.size() == 3);
    CHECK(std::holds_alternative<BattleTempAttackBuffCommand>(result.commands[0]));
    CHECK(std::holds_alternative<BattleUnitHealCommand>(result.commands[1]));
    CHECK(std::holds_alternative<BattleUnitShieldCommand>(result.commands[2]));
}

TEST_CASE("BattleDamageApplication_ReturnsBattleResultWithoutSceneTeamScan", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    queuePendingDamage(frame, damageInput(0, 1, 20));

    auto result = BattleDamageApplicationSystem().apply(input);

    CHECK(result.battleEnded);
    CHECK(result.winningTeam == 0);
    REQUIRE(findLifecycleEvent(result, BattleDamageLifecycleEventType::BattleEnded));
    CHECK(findLifecycleEvent(result, BattleDamageLifecycleEventType::BattleEnded)->value == 0);
    REQUIRE(result.gameplayEvents.size() == 2);
    CHECK(result.gameplayEvents[1].type == BattleGameplayEventType::BattleEnded);
    CHECK(result.gameplayEvents[1].frame == 77);
    CHECK(result.gameplayEvents[1].amount == 0);
}
