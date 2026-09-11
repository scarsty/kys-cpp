#include "battle/BattleEffectCommandSystem.h"
#include "battle/BattleRuntimeUnitSpawn.h"
#include "battle/BattleRuntimeEffects.h"
#include "BattleCoreTestHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <variant>
#include <vector>

using namespace KysChess;
using namespace KysChess::Battle;
using namespace KysChess::Battle::Test;

namespace
{

BattleRuntimeUnit makeUnit(
    int id,
    int team,
    int hp,
    int maxHp,
    int mp = 0,
    int maxMp = 100)
{
    BattleRuntimeUnit unit;
    unit.id = id;
    unit.team = team;
    unit.vitals = { hp, maxHp, mp, maxMp };
    unit.stats = { 100, 80, 60 };
    return unit;
}

BattleRuntimeState makeState();

EffectCommandMetadata metadata(
    int magicId,
    int targetUnitId,
    std::uint64_t commandOrdinal = 0);

TEST_CASE("BattleEffectCommandSystem status removal synchronizes typed control and current stagger", "[battle][effect][command][status][control]")
{
    SECTION("逍遙遊解除控制但保留目前動作與完整移動狀態")
    {
        auto state = makeState();
        auto& target = state.units.require(2);
        target.status.effects.statusShield = 31;
        target.status.effects.staggerShield = 37;
        target.status.effects.controlImmunityFrames = 41;
        target.status.effects.statuses = {
            {
                .kind = BattleStatusKind::Stun,
                .sourceUnitId = 3,
                .remainingFrames = 19,
                .maximumFrames = 23,
                .appliedSequence = 7,
            },
            {
                .kind = BattleStatusKind::ColdPoison,
                .sourceUnitId = 3,
                .remainingFrames = 29,
                .appliedSequence = 9,
            },
        };

        target.core.haveAction = true;
        target.core.operationType = BattleOperationType::RangedProjectile;
        target.core.animation.cooldown = 43;
        target.core.animation.cooldownMax = 47;
        target.core.animation.actFrame = 11;
        target.core.animation.actType = 5;
        target.core.motion.position = { 101.0f, 103.0f, 107.0f };
        target.core.motion.velocity = { 109.0f, 113.0f, 127.0f };
        target.core.motion.acceleration = { 131.0f, 137.0f, 139.0f };
        target.movement.active = true;
        target.movement.targetId = 3;
        target.movement.assignedSlot = 13;
        target.movement.slotSwitchCooldownRemaining = 17;
        target.movement.physics.position = { 149.0f, 151.0f, 157.0f };
        target.movement.physics.velocity = { 163.0f, 167.0f, 173.0f };
        target.movement.physics.acceleration = { 179.0f, 181.0f, 191.0f };
        target.movement.physics.knockbackVelocity = { 193.0f, 197.0f, 199.0f };
        target.movement.physics.knockbackFrames = 23;
        target.movement.physics.knockbackControlFrames = 29;
        BattlePendingCastAction pending;
        pending.targetUnitId = 3;
        pending.operationType = BattleOperationType::RangedProjectile;
        pending.castFrame = 31;
        pending.effectCast.provenance = {
            .rootCastId = BattleCastId{ 1 },
            .castId = BattleCastId{ 1 },
            .sourceUnitId = 2,
            .ultimate = true,
            .origin = CastOriginKind::Ultimate,
        };
        target.setPendingCast(std::move(pending));
        target.markUltimateCaster();

        RemoveStatusAction action;
        action.controlOnly = true;
        action.clearCurrentActionStagger = true;
        const EffectCommand command{
            metadata(2, 2),
            RemoveStatusEffectCommand{ action },
        };

        const auto reduced = BattleEffectCommandSystem().reduce(
            state,
            command,
            { .frame = 20 });

        REQUIRE(reduced.entries.size() == 1);
        const auto& removal = std::get<BattleStatusRemoveEffectResult>(
            reduced.entries[0].value);
        CHECK(removal.status.currentActionStaggerCleared);
        CHECK_FALSE(target.status.effects.has(BattleStatusKind::Stun));
        REQUIRE(target.status.effects.statuses.size() == 1);
        CHECK(target.status.effects.statuses[0].kind == BattleStatusKind::ColdPoison);
        CHECK(target.status.effects.statusShield == 31);
        CHECK(target.status.effects.staggerShield == 37);
        CHECK(target.status.effects.controlImmunityFrames == 41);

        CHECK(target.core.haveAction);
        CHECK(target.core.operationType == BattleOperationType::RangedProjectile);
        CHECK(target.core.animation.cooldown == 43);
        CHECK(target.core.animation.cooldownMax == 47);
        CHECK(target.core.animation.actFrame == 11);
        CHECK(target.core.animation.actType == 5);
        REQUIRE(target.pendingCast() != nullptr);
        CHECK(target.pendingCast()->targetUnitId == 3);
        CHECK(target.pendingCast()->castFrame == 31);
        CHECK(target.isUltimateCaster());
        CHECK(target.isSkillCooldownUltimate());

        CHECK(target.core.motion.position.x == 101.0f);
        CHECK(target.core.motion.position.y == 103.0f);
        CHECK(target.core.motion.position.z == 107.0f);
        CHECK(target.core.motion.velocity.x == 109.0f);
        CHECK(target.core.motion.acceleration.z == 139.0f);
        CHECK(target.movement.active);
        CHECK(target.movement.targetId == 3);
        CHECK(target.movement.assignedSlot == 13);
        CHECK(target.movement.slotSwitchCooldownRemaining == 17);
        CHECK(target.movement.physics.position.x == 149.0f);
        CHECK(target.movement.physics.position.y == 151.0f);
        CHECK(target.movement.physics.position.z == 157.0f);
        CHECK(target.movement.physics.velocity.y == 167.0f);
        CHECK(target.movement.physics.acceleration.z == 191.0f);
        CHECK(target.movement.physics.knockbackVelocity.x == 193.0f);
        CHECK(target.movement.physics.knockbackFrames == 23);
        CHECK(target.movement.physics.knockbackControlFrames == 29);
    }

    SECTION("清除全部負面狀態也同步解除既有僵直且不消耗防護")
    {
        auto state = makeState();
        auto& target = state.units.require(2);
        target.status.effects.statusShield = 50;
        target.status.effects.staggerShield = 60;
        target.status.effects.controlImmunityFrames = 70;
        target.status.effects.statuses = {
            {
                .kind = BattleStatusKind::Poison,
                .sourceUnitId = 3,
                .remainingFrames = 30,
                .stacks = 1,
                .appliedSequence = 3,
            },
            {
                .kind = BattleStatusKind::Stun,
                .sourceUnitId = 3,
                .remainingFrames = 12,
                .maximumFrames = 18,
                .appliedSequence = 4,
            },
            {
                .kind = BattleStatusKind::WitheredBone,
                .remainingFrames = 90,
                .appliedSequence = 5,
            },
            {
                .kind = BattleStatusKind::Shadowless,
                .remainingFrames = 100,
                .appliedSequence = 6,
            },
            {
                .kind = BattleStatusKind::NextAttackMiss,
                .remainingFrames = 120,
                .appliedSequence = 7,
            },
        };

        RemoveStatusAction action;
        action.negativeOnly = true;
        const EffectCommand command{
            metadata(107, 2),
            RemoveStatusEffectCommand{ action },
        };

        const auto reduced = BattleEffectCommandSystem().reduce(
            state,
            command,
            { .frame = 20 });

        const auto& removal = std::get<BattleStatusRemoveEffectResult>(
            reduced.entries[0].value);
        CHECK(removal.status.currentActionStaggerCleared);
        CHECK_FALSE(target.status.effects.has(BattleStatusKind::Poison));
        CHECK_FALSE(target.status.effects.has(BattleStatusKind::Stun));
        REQUIRE(target.status.effects.statuses.size() == 2);
        CHECK(target.status.effects.statuses[0].kind == BattleStatusKind::Shadowless);
        CHECK(target.status.effects.statuses[1].kind == BattleStatusKind::NextAttackMiss);
        CHECK(target.status.effects.statusShield == 50);
        CHECK(target.status.effects.staggerShield == 60);
        CHECK(target.status.effects.controlImmunityFrames == 70);
    }

    SECTION("單獨解除動作僵直不會誤清其他負面效果")
    {
        auto state = makeState();
        auto& target = state.units.require(2);
        target.status.effects.statuses = {
            {
                .kind = BattleStatusKind::Stun,
                .remainingFrames = 12,
                .maximumFrames = 12,
            },
            {
                .kind = BattleStatusKind::Poison,
                .remainingFrames = 30,
                .stacks = 1,
            },
        };
        RemoveStatusAction action;
        action.clearCurrentActionStagger = true;
        const EffectCommand command{
            metadata(2, 2),
            RemoveStatusEffectCommand{ action },
        };

        BattleEffectCommandSystem().reduce(state, command, { .frame = 20 });

        CHECK_FALSE(target.status.effects.has(BattleStatusKind::Stun));
        CHECK(target.status.effects.remainingFrames(BattleStatusKind::Poison) == 30);
    }

    SECTION("狀態盾與僵直盾依序吸收控制並保留各自結果語意")
    {
        auto state = makeState();
        auto& target = state.units.require(2);
        target.status.effects.statusShield = 10;
        target.status.effects.staggerShield = 20;
        ApplyStatusAction action;
        action.status = BattleStatusKind::Stun;
        action.durationFrames = 25;
        action.quantity = NoStatusQuantity{};
        action.reapplication = StatusReapplicationPolicy::KeepLongerDuration;
        const EffectCommand command{
            metadata(92, 2),
            ApplyStatusEffectCommand{ action, std::nullopt },
        };

        const auto reduced = BattleEffectCommandSystem().reduce(
            state,
            command,
            {
                .frame = 20,
                .controlLowHpImmunityPct = 0,
            });

        const auto& applied = std::get<BattleStatusApplyEffectResult>(
            reduced.entries[0].value).status;
        CHECK(applied.outcome == BattleStatusApplyOutcome::BlockedByStaggerShield);
        CHECK(applied.statusShieldAbsorbed == 10);
        CHECK(applied.staggerShieldAbsorbed == 15);
        CHECK_FALSE(applied.applied);
        CHECK(target.status.effects.statusShield == 0);
        CHECK(target.status.effects.staggerShield == 5);
        CHECK_FALSE(target.status.effects.has(BattleStatusKind::Stun));
    }

    SECTION("友軍殘影是正面狀態且不消耗狀態盾")
    {
        auto state = makeState();
        auto& target = state.units.require(2);
        target.status.effects.statusShield = 50;
        ApplyStatusAction action;
        action.status = BattleStatusKind::NextAttackMiss;
        action.durationFrames = 120;
        action.quantity = SetStatusTriggerCharges{ 1 };
        action.behavior = attackSuppressionStatusBehavior(
            BattleStatusKind::NextAttackMiss);
        const EffectCommand command{
            metadata(69, 2),
            ApplyStatusEffectCommand{ action, std::nullopt },
        };

        const auto reduced = BattleEffectCommandSystem().reduce(
            state,
            command,
            { .frame = 20 });

        const auto& applied = std::get<BattleStatusApplyEffectResult>(
            reduced.entries[0].value).status;
        CHECK(applied.outcome == BattleStatusApplyOutcome::Applied);
        CHECK(applied.applied);
        CHECK(applied.statusShieldAbsorbed == 0);
        CHECK(target.status.effects.statusShield == 50);
        const auto snapshot = BattleStatusSystem({}).snapshot(
            target.statusDamageState());
        CHECK(snapshot.has(BattleStatusKind::NextAttackMiss));
    }

    SECTION("中毒命令提交取代並保留較高傷害")
    {
        auto state = makeState();
        auto& target = state.units.require(2);
        ApplyStatusAction replace;
        replace.status = BattleStatusKind::Poison;
        replace.durationFrames = 120;
        replace.quantity = SetStatusTriggerCharges{ 4 };
        replace.reapplication = StatusReapplicationPolicy::ReplaceExistingPoison;
        replace.behavior = poisonStatusBehavior(10);
        const EffectCommand replaceCommand{
            metadata(21, 2),
            ApplyStatusEffectCommand{ replace, std::nullopt },
        };

        const auto replaced = BattleEffectCommandSystem().reduce(
            state,
            replaceCommand,
            { .frame = 20 });

        const auto& replaceResult = std::get<BattleStatusApplyEffectResult>(
            replaced.entries[0].value).status;
        REQUIRE(replaceResult.applied);
        CHECK(replaceResult.outcome == BattleStatusApplyOutcome::Applied);
        REQUIRE(target.status.effects.find(BattleStatusKind::Poison));
        CHECK(target.status.effects.find(BattleStatusKind::Poison)->stacks == 4);
        CHECK(target.status.effects.find(BattleStatusKind::Poison)->remainingFrames == 120);
        CHECK(poisonDamagePercent(
            target.status.effects.find(BattleStatusKind::Poison)->behavior) == 10);

        ApplyStatusAction add = replace;
        add.durationFrames = 150;
        add.quantity = SetStatusTriggerCharges{ 5 };
        add.reapplication = StatusReapplicationPolicy::KeepHigherDamage;
        add.behavior = poisonStatusBehavior(12);
        const EffectCommand addCommand{
            metadata(21, 2, 1),
            ApplyStatusEffectCommand{ add, std::nullopt },
        };

        const auto stacked = BattleEffectCommandSystem().reduce(
            state,
            addCommand,
            { .frame = 21 });

        const auto& stackResult = std::get<BattleStatusApplyEffectResult>(
            stacked.entries[0].value).status;
        REQUIRE(stackResult.applied);
        CHECK(stackResult.outcome == BattleStatusApplyOutcome::Replaced);
        REQUIRE(target.status.effects.find(BattleStatusKind::Poison));
        CHECK(target.status.effects.find(BattleStatusKind::Poison)->stacks == 5);
        CHECK(target.status.effects.find(BattleStatusKind::Poison)->remainingFrames == 150);
        CHECK(poisonDamagePercent(
            target.status.effects.find(BattleStatusKind::Poison)->behavior) == 12);
        const auto snapshot = BattleStatusSystem({}).snapshot(
            target.statusDamageState());
        CHECK(snapshot.stacks(BattleStatusKind::Poison) == 5);
    }
}

TEST_CASE("BattleEffectCommandSystem observes every repeated poison application through status shields",
          "[battle][effect][command][status][poison_explosion]")
{
    auto state = makeState();
    auto& target = state.units.require(2);
    target.status.effects.statusShield = 300;
    ApplyStatusAction poison;
    poison.status = BattleStatusKind::Poison;
    poison.durationFrames = 120;
    poison.quantity = SetStatusTriggerCharges{ 4 };
    poison.reapplication = StatusReapplicationPolicy::ReplaceExistingPoison;
    poison.behavior = poisonStatusBehavior(10);
    const std::array commands{
        EffectCommand{ metadata(95, 2, 0), ApplyStatusEffectCommand{ poison, std::nullopt } },
        EffectCommand{ metadata(95, 2, 1), ApplyStatusEffectCommand{ poison, std::nullopt } },
        EffectCommand{ metadata(95, 2, 2), ApplyStatusEffectCommand{ poison, std::nullopt } },
    };

    const auto reduced = BattleEffectCommandSystem().reduce(
        state,
        commands,
        { .frame = 20 });

    REQUIRE(reduced.entries.size() == 3);
    CHECK(std::get<BattleStatusApplyEffectResult>(reduced.entries[0].value)
              .status.outcome == BattleStatusApplyOutcome::BlockedByStatusShield);
    CHECK(std::get<BattleStatusApplyEffectResult>(reduced.entries[1].value)
              .status.outcome == BattleStatusApplyOutcome::BlockedByStatusShield);
    const auto& third = std::get<BattleStatusApplyEffectResult>(
        reduced.entries[2].value).status;
    CHECK(third.outcome == BattleStatusApplyOutcome::Applied);
    CHECK(third.appliedDurationFrames == 60);
    CHECK(target.status.effects.statusShield == 0);
    REQUIRE(target.status.effects.find(BattleStatusKind::Poison));
    CHECK(target.status.effects.find(BattleStatusKind::Poison)->stacks == 4);
    CHECK(target.status.effects.find(BattleStatusKind::Poison)->remainingFrames == 60);
}

TEST_CASE("BattleEffectCommandSystem does not turn an aggregated poison clock into producer-family capacity",
          "[battle][effect][command][status][poison][aggregation][capacity]")
{
    auto state = makeState();
    auto& target = state.units.require(2);

    ApplyStatusAction aggregated;
    aggregated.status = BattleStatusKind::Poison;
    aggregated.durationFrames = 120;
    aggregated.quantity = SetStatusTriggerCharges{ 5 };
    aggregated.reapplication = StatusReapplicationPolicy::KeepHigherDamage;
    aggregated.behavior = poisonStatusBehavior(12);
    const EffectCommand aggregatedCommand{
        metadata(21, 2),
        ApplyStatusEffectCommand{ aggregated, std::nullopt },
    };
    const auto first = BattleEffectCommandSystem().reduce(
        state,
        aggregatedCommand,
        { .frame = 20 });

    REQUIRE(first.entries.size() == 1);
    REQUIRE(target.status.effects.statuses.size() == 1);
    CHECK_FALSE(target.status.effects.statuses.front().familyLocalLimit);
    CHECK(target.status.effects.statuses.front().stacks == 5);

    ApplyStatusAction producerAlone = aggregated;
    producerAlone.quantity = SetStatusTriggerCharges{ 3 };
    producerAlone.behavior = poisonStatusBehavior(14);
    const EffectCommand producerAloneCommand{
        metadata(21, 2, 1),
        ApplyStatusEffectCommand{ producerAlone, std::nullopt },
    };
    const auto second = BattleEffectCommandSystem().reduce(
        state,
        producerAloneCommand,
        { .frame = 21 });

    REQUIRE(second.entries.size() == 1);
    const auto& applied = std::get<BattleStatusApplyEffectResult>(
        second.entries.front().value).status;
    CHECK(applied.outcome == BattleStatusApplyOutcome::Replaced);
    REQUIRE(target.status.effects.statuses.size() == 1);
    CHECK(target.status.effects.statuses.front().stacks == 3);
    CHECK_FALSE(target.status.effects.statuses.front().familyLocalLimit);
    CHECK(poisonDamagePercent(target.status.effects.statuses.front().behavior) == 14);
}

TEST_CASE("BattleEffectCommandSystem preserves all stun reapplication policies",
          "[battle][effect][command][status][stun][reapplication]")
{
    auto state = makeState();
    BattleEffectCommandSystem system;
    std::uint64_t ordinal{};
    const auto apply = [&](StatusReapplicationPolicy policy, int duration)
    {
        ApplyStatusAction action;
        action.status = BattleStatusKind::Stun;
        action.durationFrames = duration;
        action.quantity = NoStatusQuantity{};
        action.reapplication = policy;
        const EffectCommand command{
            metadata(92, 2, ordinal++),
            ApplyStatusEffectCommand{ action, std::nullopt },
        };
        const auto reduced = system.reduce(
            state,
            command,
            {
                .frame = 20,
                .controlLowHpImmunityPct = 0,
            });
        REQUIRE(reduced.entries.size() == 1);
        return std::get<BattleStatusApplyEffectResult>(
            reduced.entries.front().value).status;
    };
    const auto activeStun = [&]() -> const BattleStatusContribution&
    {
        const auto* stun = state.units.require(2).status.effects.find(
            BattleStatusKind::Stun);
        REQUIRE(stun);
        return *stun;
    };

    const auto initial = apply(
        StatusReapplicationPolicy::KeepLongerDuration, 10);
    CHECK(initial.applied);
    CHECK(initial.outcome == BattleStatusApplyOutcome::Applied);
    CHECK(activeStun().remainingFrames == 10);
    CHECK(activeStun().maximumFrames == 10);

    const auto extended = apply(
        StatusReapplicationPolicy::ExtendDuration, 4);
    CHECK(extended.applied);
    CHECK(extended.value == 4);
    CHECK(activeStun().remainingFrames == 14);
    CHECK(activeStun().maximumFrames == 14);

    const auto keptExisting = apply(
        StatusReapplicationPolicy::KeepLongerDuration, 8);
    CHECK_FALSE(keptExisting.applied);
    CHECK(keptExisting.value == 0);
    CHECK(activeStun().remainingFrames == 14);

    const auto keptIncoming = apply(
        StatusReapplicationPolicy::KeepLongerDuration, 20);
    CHECK(keptIncoming.applied);
    CHECK(keptIncoming.value == 6);
    CHECK(activeStun().remainingFrames == 20);
    CHECK(activeStun().maximumFrames == 20);

}

TEST_CASE("BattleEffectCommandSystem bounds duration-only behavior aliases to one family generation",
          "[battle][effect][command][status][family][alias]")
{
    ApplyStatusAction shadowless;
    shadowless.status = BattleStatusKind::Shadowless;
    shadowless.durationFrames = 60;
    shadowless.quantity = NoStatusQuantity{};
    shadowless.reapplication = StatusReapplicationPolicy::RefreshDuration;
    shadowless.behavior = trueQiStatusBehavior(9);
    const auto firstBehavior = shadowless.behavior;

    auto firstMetadata = metadata(106, 2);
    firstMetadata.ruleId = EffectRuleId{ 500 };
    firstMetadata.ruleOrder = 7;
    firstMetadata.authoredActionOrder = 3;
    firstMetadata.binding.runtimeInstanceId = 11;
    BattleStatusUnitState target{
        .id = 2,
        .alive = true,
        .hp = 100,
        .maxHp = 100,
    };
    auto first = BattleEffectCommandSystem::applyStatusCommand(
        std::move(target),
        firstMetadata,
        ApplyStatusEffectCommand{ shadowless, std::nullopt },
        { .frame = 1 },
        {},
        false);
    REQUIRE(first.target.effects.statuses.size() == 1);
    const auto firstSequence = first.target.effects.statuses.front().appliedSequence;
    CHECK(first.target.effects.statuses.front().familyLocalLimit == 1);

    auto aliasMetadata = firstMetadata;
    aliasMetadata.binding.runtimeInstanceId = 12;
    shadowless.durationFrames = 90;
    shadowless.behavior = trueQiStatusBehavior(12);
    auto alias = BattleEffectCommandSystem::applyStatusCommand(
        std::move(first.target),
        aliasMetadata,
        ApplyStatusEffectCommand{ shadowless, std::nullopt },
        { .frame = 2 },
        {},
        false);
    CHECK(alias.outcome == BattleStatusApplyOutcome::Refreshed);
    REQUIRE(alias.target.effects.statuses.size() == 1);
    const auto& refreshedOriginal = alias.target.effects.statuses.front();
    REQUIRE(refreshedOriginal.producer);
    CHECK(refreshedOriginal.producer->binding.runtimeInstanceId == 11);
    CHECK(refreshedOriginal.remainingFrames == 90);
    CHECK(refreshedOriginal.appliedSequence == firstSequence);
    CHECK(statusBehaviorsEquivalent(refreshedOriginal.behavior, firstBehavior));

    shadowless.durationFrames = 120;
    auto refresh = BattleEffectCommandSystem::applyStatusCommand(
        std::move(alias.target),
        aliasMetadata,
        ApplyStatusEffectCommand{ shadowless, std::nullopt },
        { .frame = 3 },
        {},
        false);
    CHECK(refresh.outcome == BattleStatusApplyOutcome::Refreshed);
    REQUIRE(refresh.target.effects.statuses.size() == 1);
    CHECK(refresh.target.effects.statuses.front().appliedSequence
        == firstSequence);
    REQUIRE(refresh.target.effects.statuses.front().producer);
    CHECK(refresh.target.effects.statuses.front().producer->binding.runtimeInstanceId
        == 11);
    CHECK(statusBehaviorsEquivalent(
        refresh.target.effects.statuses.front().behavior,
        firstBehavior));
    CHECK(refresh.target.effects.statuses.front().remainingFrames == 120);
}

BattleRuntimeState makeState()
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    appendRuntimeUnit(state, makeRuntimeUnitSpawn(makeUnit(1, 0, 900, 1000, 20), {}));
    appendRuntimeUnit(state, makeRuntimeUnitSpawn(makeUnit(2, 0, 200, 1000, 10), {}));
    appendRuntimeUnit(state, makeRuntimeUnitSpawn(makeUnit(3, 1, 1000, 1000, 80), {}));
    return state;
}

EffectCommandMetadata metadata(
    int magicId,
    int targetUnitId,
    std::uint64_t commandOrdinal)
{
    return {
        .binding = {
            .kind = EffectSourceKind::Magic,
            .sourceId = magicId,
            .ownerUnitId = 1,
            .sourceTeam = 0,
        },
        .ruleId = EffectRuleId{ static_cast<std::uint64_t>(magicId) },
        .event = EffectEvent::UltimateCommitted,
        .commandOrdinal = commandOrdinal,
        .targetUnitId = targetUnitId,
    };
}

EffectCommand resourceCommand(
    int magicId,
    int targetUnitId,
    BattleResource resource,
    ResourceChangeKind kind,
    int amount,
    std::uint64_t commandOrdinal = 0)
{
    ChangeResourceAction action;
    action.resource = resource;
    action.kind = kind;
    return {
        metadata(magicId, targetUnitId, commandOrdinal),
        ChangeResourceEffectCommand{ action, amount },
    };
}

EffectCommand damageModifierCommand(
    int targetUnitId,
    DamageModifierOperation operation,
    int amount,
    int durationFrames,
    EffectStackPolicy stack = EffectStackPolicy::Independent,
    std::uint32_t actionOrder = 0,
    DamageChannel channel = DamageChannel::Skill,
    DamageModifierStage stage = DamageModifierStage::BeforeDefense,
    std::optional<int> stackLimit = std::nullopt)
{
    ModifyDamageAction action;
    action.stage = stage;
    action.channel = channel;
    action.operation = operation;
    action.durationFrames = durationFrames;
    action.stack = stack;
    action.stackLimit = stackLimit;
    auto commandMetadata = metadata(80, targetUnitId);
    commandMetadata.actionOrder = actionOrder;
    return {
        commandMetadata,
        ModifyDamageEffectCommand{ action, amount },
    };
}

EffectCommand damageAbsorptionCommand(
    int targetUnitId,
    int durationFrames = 80,
    std::uint32_t actionOrder = 0)
{
    StartDamageAbsorptionAction action;
    action.slot = EffectStateSlot::AbsorbedDamage;
    action.absorbedPct = 40;
    action.durationFrames = durationFrames;
    action.settleOnSourceDeath = true;
    action.settlementTarget.kind = EffectSelectorKind::Enemies;
    action.settlementTarget.count = 1;
    action.settlementTarget.tieBreak = EffectTieBreak::BattleRandom;
    action.settlementDamageKind = BattleDamageKind::Pure;
    action.returnedPct = 100;
    auto commandMetadata = metadata(97, targetUnitId);
    commandMetadata.actionOrder = actionOrder;
    StateMachineEffectCommand command;
    command.action = StateMachineAction{ action };
    return { commandMetadata, std::move(command) };
}

}  // namespace

TEST_CASE("BattleEffectCommandSystem skips a snapshotted status command after its contribution is removed",
          "[battle][effect][command][status][snapshot][liveness]")
{
    auto state = makeState();
    auto& effects = state.units.require(2).status.effects;
    effects.statuses.push_back({
        .kind = BattleStatusKind::TrueQi,
        .stacks = 1,
        .appliedSequence = 7,
    });

    RemoveStatusAction removal;
    removal.statuses = { BattleStatusKind::TrueQi };
    EffectCommand removeCommand{
        metadata(106, 2, 0),
        RemoveStatusEffectCommand{ removal },
    };

    DealDamageAction damage;
    damage.amount.flat = 9;
    damage.kind = BattleDamageKind::Pure;
    auto damageMetadata = metadata(106, 3, 1);
    damageMetadata.executionLane = EffectExecutionLane::StatusBehavior;
    damageMetadata.statusContribution = EffectStatusContributionContext{
        .holderUnitId = 2,
        .sourceUnitId = 1,
        .kind = BattleStatusKind::TrueQi,
        .quantity = 1,
        .appliedSequence = 7,
        .producerRuleId = EffectRuleId{ 106 },
    };
    const EffectCommand damageCommand{
        damageMetadata,
        DealDamageEffectCommand{ damage, 9 },
    };
    const std::array commands{ removeCommand, damageCommand };

    const auto reduced = BattleEffectCommandSystem().reduce(
        state,
        commands,
        { .frame = 1 });

    REQUIRE(reduced.entries.size() == 2);
    CHECK(std::holds_alternative<BattleStatusRemoveEffectResult>(
        reduced.entries[0].value));
    CHECK(std::holds_alternative<BattleSkippedEffectResult>(
        reduced.entries[1].value));
    CHECK_FALSE(effects.has(BattleStatusKind::TrueQi));
}

TEST_CASE("BattleEffectCommandSystem preserves complete status damage origin and trigger lineage",
          "[battle][effect][command][status][damage][provenance]")
{
    const EffectSourceBinding firstBinding{
        .kind = EffectSourceKind::Magic,
        .sourceId = 106,
        .ownerUnitId = 1,
        .sourceTeam = 0,
        .runtimeInstanceId = 17,
    };
    const BattleAttackProvenance attack{
        .cast = {
            .rootCastId = BattleCastId{ 31 },
            .castId = BattleCastId{ 31 },
            .sourceUnitId = 1,
            .magicId = 106,
            .ultimate = true,
        },
        .attackId = BattleAttackId{ 32 },
        .rootAttack = true,
        .mainProjectile = true,
    };
    BattleEffectDamageRequestOutput first;
    first.source = firstBinding;
    first.ruleId = EffectRuleId{ 700 };
    first.triggeringCast = attack.cast;
    first.triggeringAttack = attack;
    first.statusContribution = EffectStatusContributionContext{
        .holderUnitId = 3,
        .sourceUnitId = 1,
        .kind = BattleStatusKind::TrueQi,
        .quantity = 4,
        .appliedSequence = 41,
        .producerRuleId = EffectRuleId{ 700 },
        .producerRuleOrder = 11,
        .producerActionOrder = 2,
        .behaviorRuleOrder = 5,
    };
    first.authoredActionOrder = 7;

    const auto firstOrigin = BattleEffectCommandSystem::damageOrigin(first);
    const auto* status = std::get_if<EffectStatusDamageOrigin>(&firstOrigin);
    REQUIRE(status);
    CHECK(status->binding == firstBinding);
    CHECK(status->contribution == *first.statusContribution);
    CHECK(status->behaviorActionOrder == 7);
    REQUIRE(status->triggeringCast);
    REQUIRE(status->triggeringAttack);
    CHECK(status->triggeringCast->castId == BattleCastId{ 31 });
    CHECK(status->triggeringAttack->attackId == BattleAttackId{ 32 });
    REQUIRE(effectDamageCastProvenance(firstOrigin));
    REQUIRE(effectDamageAttackProvenance(firstOrigin));
    CHECK(effectDamageCastProvenance(firstOrigin)->ultimate);

    auto second = first;
    second.source.sourceId = 206;
    second.source.runtimeInstanceId = 19;
    second.statusContribution->appliedSequence = 43;
    second.statusContribution->producerRuleId = EffectRuleId{ 701 };
    second.triggeringCast.reset();
    second.triggeringAttack.reset();
    const auto secondOrigin = BattleEffectCommandSystem::damageOrigin(second);
    const auto* secondStatus = std::get_if<EffectStatusDamageOrigin>(&secondOrigin);
    REQUIRE(secondStatus);
    CHECK(secondStatus->binding != status->binding);
    CHECK(secondStatus->contribution.appliedSequence
        != status->contribution.appliedSequence);
    CHECK(effectDamageCastProvenance(secondOrigin) == nullptr);
    CHECK(effectDamageAttackProvenance(secondOrigin) == nullptr);

    auto settlement = first;
    settlement.statusContribution.reset();
    const auto settlementOrigin = BattleEffectCommandSystem::damageOrigin(settlement);
    const auto* rule = std::get_if<EffectRuleDamageOrigin>(&settlementOrigin);
    REQUIRE(rule);
    CHECK(rule->binding == firstBinding);
    CHECK(rule->ruleId == EffectRuleId{ 700 });
    CHECK(rule->actionOrder == 7);
    REQUIRE(effectDamageAttackProvenance(settlementOrigin));
}

TEST_CASE("BattleEffectCommandSystem invalidates a snapshotted status command after partial consumption",
          "[battle][effect][command][status][snapshot][liveness]")
{
    auto state = makeState();
    auto& effects = state.units.require(2).status.effects;
    effects.statuses.push_back({
        .kind = BattleStatusKind::TrueQi,
        .sourceUnitId = 1,
        .stacks = 2,
        .appliedSequence = 7,
    });

    auto statusMetadata = metadata(106, 2, 0);
    statusMetadata.executionLane = EffectExecutionLane::StatusBehavior;
    statusMetadata.statusContribution = EffectStatusContributionContext{
        .holderUnitId = 2,
        .sourceUnitId = 1,
        .kind = BattleStatusKind::TrueQi,
        .quantity = 2,
        .appliedSequence = 7,
        .producerRuleId = EffectRuleId{ 106 },
    };
    ConsumeThisStatusAction consume;
    consume.quantity = 1;
    const EffectCommand consumeCommand{
        statusMetadata,
        ConsumeThisStatusEffectCommand{ consume },
    };

    DealDamageAction damage;
    damage.amount.flat = 9;
    damage.kind = BattleDamageKind::Pure;
    auto damageMetadata = statusMetadata;
    damageMetadata.targetUnitId = 3;
    damageMetadata.actionOrder = 1;
    const EffectCommand damageCommand{
        damageMetadata,
        DealDamageEffectCommand{ damage, 9 },
    };
    const std::array commands{ consumeCommand, damageCommand };

    const auto reduced = BattleEffectCommandSystem().reduce(
        state,
        commands,
        { .frame = 1 });

    REQUIRE(reduced.entries.size() == 2);
    CHECK(std::holds_alternative<BattleStatusConsumeEffectResult>(
        reduced.entries[0].value));
    CHECK(std::holds_alternative<BattleSkippedEffectResult>(
        reduced.entries[1].value));
    REQUIRE(effects.statuses.size() == 1);
    CHECK(effects.statuses.front().stacks == 1);
}

TEST_CASE("BattleEffectCommandSystem reduces the first ultimate vertical slices", "[battle][effect][command]")
{
    BattleEffectCommandSystem system;

    SECTION("青囊奇術治療走唯一治療交易")
    {
        auto state = makeState();
        const auto command = resourceCommand(
            127,
            2,
            BattleResource::Hp,
            ResourceChangeKind::Restore,
            70);

        const auto reduced = system.reduce(state, command, { .frame = 10 });

        REQUIRE(reduced.entries.size() == 1);
        const auto& resource = std::get<BattleResourceEffectResult>(reduced.entries[0].value);
        REQUIRE(resource.heal);
        CHECK(resource.heal->outcome == BattleHealOutcome::Applied);
        CHECK(resource.heal->appliedAmount == 70);
        CHECK(state.units.requireCore(2).vitals.hp == 270);
        CHECK(state.heals.committedTransactions.size() == 1);
        REQUIRE(state.heals.events.size() == 2);
        CHECK(state.heals.events[0].type == BattleHealEventType::Attempted);
        CHECK(state.heals.events[1].type == BattleHealEventType::Applied);
    }

    SECTION("神照功護盾直接提交至 runtime")
    {
        auto state = makeState();
        state.units.requireCore(1).shield = 25;
        const auto command = resourceCommand(
            94,
            1,
            BattleResource::Shield,
            ResourceChangeKind::Grant,
            300);

        const auto reduced = system.reduce(state, command, { .frame = 10 });

        REQUIRE(reduced.entries.size() == 1);
        const auto& resource = std::get<BattleResourceEffectResult>(reduced.entries[0].value);
        CHECK(resource.outcome == BattleResourceEffectOutcome::Applied);
        REQUIRE(resource.deltas.size() == 1);
        CHECK(resource.deltas[0].before == 25);
        CHECK(resource.deltas[0].after == 325);
        CHECK(state.units.requireCore(1).shield == 325);
    }

    SECTION("九陰白骨爪套用枯骨 typed status")
    {
        auto state = makeState();
        ApplyStatusAction action;
        action.status = BattleStatusKind::WitheredBone;
        action.durationFrames = 120;
        action.quantity = NoStatusQuantity{};
        action.behavior = makeCatalogOwnedStatusBehavior(action);
        const EffectCommand command{
            metadata(11, 3),
            ApplyStatusEffectCommand{ action, std::nullopt },
        };

        const auto reduced = system.reduce(state, command, { .frame = 10 });

        REQUIRE(reduced.entries.size() == 1);
        const auto& applied = std::get<BattleStatusApplyEffectResult>(reduced.entries[0].value);
        CHECK(applied.status.applied);
        const auto snapshot = BattleStatusSystem({}).snapshot(
            state.units.require(3).statusDamageState());
        CHECK(snapshot.has(BattleStatusKind::WitheredBone));
        CHECK(snapshot.damageTakenPct == 25);
        REQUIRE(snapshot.healTransactionModifiers.size() == 1);
        CHECK(snapshot.healTransactionModifiers.front().operation
              == HealModifierOperation::MultiplyReceived);
        CHECK(snapshot.healTransactionModifiers.front().percent == 25);
    }

    SECTION("黃沙命中位置建立持續區域")
    {
        auto state = makeState();
        AreaModifier modifier;
        modifier.kind = AreaModifierKind::Attribute;
        modifier.relation = EffectTeamFilter::Enemy;
        modifier.attribute = BattleAttribute::Speed;
        modifier.amount.flat = -25;
        modifier.overlap = AreaOverlapPolicy::KeepStrongest;
        CreateAreaAction action;
        action.shape = AreaShape::Circle;
        action.radiusTiles = 6;
        action.anchor = AreaAnchor::HitPosition;
        action.durationFrames = 100;
        action.sourceDeath = AreaSourceDeathPolicy::PersistUntilExpiry;
        action.merge = AreaMergePolicy::RefreshSameSource;
        action.modifiers.push_back(modifier);
        const EffectCommand command{
            metadata(78, 3),
            CreateAreaEffectCommand{ action, { -25 } },
        };
        const Pointf hitPosition{ 120.0f, 240.0f, 0.0f };

        const auto reduced = system.reduce(state, command, {
            .frame = 30,
            .effectPosition = hitPosition,
        });

        REQUIRE(reduced.entries.size() == 1);
        const auto& created = std::get<BattleAreaEffectResult>(reduced.entries[0].value);
        CHECK(created.area.areaId.isValid());
        REQUIRE(state.areas.areas.size() == 1);
        const auto& area = state.areas.areas[0];
        CHECK(area.anchor.fixedPosition.x == hitPosition.x);
        CHECK(area.anchor.fixedPosition.y == hitPosition.y);
        CHECK(area.expiresFrameExclusive == 130);
        REQUIRE(area.modifiers.size() == 1);
        CHECK(area.modifiers[0].amount.flat == -25);
        CHECK(area.modifiers[0].amount.percent == 0);
    }
}

TEST_CASE("BattleEffectCommandSystem preserves order and queries stacked attributes", "[battle][effect][command]")
{
    auto state = makeState();
    ModifyAttributeAction action;
    action.attribute = BattleAttribute::CriticalChance;
    action.operation = AttributeOperation::PercentagePointAdd;
    action.stack = EffectStackPolicy::AddStack;
    action.stackLimit = 5;

    const std::vector<EffectCommand> commands{
        { metadata(44, 1, 8), ModifyAttributeEffectCommand{ action, 7 } },
        { metadata(44, 1, 9), ModifyAttributeEffectCommand{ action, 7 } },
    };

    const auto reduced = BattleEffectCommandSystem().reduce(
        state,
        commands,
        { .frame = 40 });

    REQUIRE(reduced.entries.size() == 2);
    CHECK(reduced.entries[0].inputOrder == 0);
    CHECK(reduced.entries[0].metadata.commandOrdinal == 8);
    CHECK(reduced.entries[1].inputOrder == 1);
    CHECK(reduced.entries[1].metadata.commandOrdinal == 9);
    REQUIRE(state.effectCommands.attributeModifiers.size() == 1);
    CHECK(state.effectCommands.attributeModifiers[0].stackCount == 2);
    CHECK(BattleEffectCommandSystem::queryAttribute(state, {
        .unitId = 1,
        .attribute = BattleAttribute::CriticalChance,
        .baseValue = 100,
        .frame = 40,
    }) == 114);
    CHECK(BattleEffectCommandSystem::queryAttribute(state, {
        .unitId = 1,
        .attribute = BattleAttribute::CriticalChance,
        .baseValue = 0,
        .frame = 40,
    }) == 14);
}

TEST_CASE("runtime attack floors stacked debuffs after all modifiers and recovers on expiry", "[battle][effect][command][attack]")
{
    auto state = makeState();
    state.movement.frame = 40;
    ModifyAttributeAction action;
    action.attribute = BattleAttribute::Attack;
    action.operation = AttributeOperation::FlatAdd;
    action.durationFrames = 1;
    action.stack = EffectStackPolicy::AddStack;
    action.stackLimit = 10;
    BattleEffectCommandSystem system;
    for (int stack = 0; stack < 10; ++stack)
        system.reduce(state, EffectCommand{
            metadata(44, 1), ModifyAttributeEffectCommand{ action, -22 },
        }, { .frame = 40 });

    REQUIRE(state.effectCommands.attributeModifiers.size() == 1);
    CHECK(state.effectCommands.attributeModifiers.front().stackCount == 10);
    CHECK(BattleEffectCommandSystem::queryAttribute(state, {
        .unitId = 1, .attribute = BattleAttribute::Attack,
        .baseValue = 100, .frame = 40,
    }) == -120);
    const int attack = effectAdjustedAttribute(state, 1, BattleAttribute::Attack, 100);
    CHECK(attack == 0);
    CHECK(BattleDamageSystem().snapshotAttackPotency(attack, 50).effectiveAttack == 0);

    SECTION("增益與減益相加後才取下限")
    {
        action.durationFrames = 2;
        system.reduce(state, EffectCommand{
            metadata(45, 1), ModifyAttributeEffectCommand{ action, 150 },
        }, { .frame = 40 });
        CHECK(effectAdjustedAttribute(state, 1, BattleAttribute::Attack, 100) == 30);
    }
    SECTION("百分比減攻也能超過基礎值")
    {
        state.effectCommands.attributeModifiers.clear();
        action.operation = AttributeOperation::PercentAdd;
        system.reduce(state, EffectCommand{
            metadata(45, 1), ModifyAttributeEffectCommand{ action, -150 },
        }, { .frame = 40 });
        CHECK(effectAdjustedAttribute(state, 1, BattleAttribute::Attack, 100) == 0);
    }
    SECTION("減攻到期後恢復原值")
    {
        state.movement.frame = 41;
        CHECK(effectAdjustedAttribute(state, 1, BattleAttribute::Attack, 100) == 100);
    }
}

TEST_CASE("BattleEffectCommandSystem refresh domains are shared across effect owners", "[battle][effect][command][modifier][refresh]")
{
    SECTION("屬性修正依效果來源刷新，事件來源範圍仍各自獨立")
    {
        auto state = makeState();
        ModifyAttributeAction action;
        action.attribute = BattleAttribute::Defence;
        action.operation = AttributeOperation::PercentAdd;
        action.durationFrames = 90;
        action.stack = EffectStackPolicy::Refresh;

        auto firstMetadata = metadata(59, 3);
        firstMetadata.eventSourceUnitId = 1;
        auto secondMetadata = firstMetadata;
        secondMetadata.binding.ownerUnitId = 2;
        secondMetadata.binding.runtimeInstanceId = 17;
        secondMetadata.eventSourceUnitId = 2;

        BattleEffectCommandSystem system;
        system.reduce(
            state,
            EffectCommand{ firstMetadata, ModifyAttributeEffectCommand{ action, -20 } },
            { .frame = 10 });
        const auto refreshed = system.reduce(
            state,
            EffectCommand{ secondMetadata, ModifyAttributeEffectCommand{ action, -30 } },
            { .frame = 40 });

        REQUIRE(state.effectCommands.attributeModifiers.size() == 1);
        CHECK(std::get<BattleAttributeEffectResult>(
                  refreshed.entries[0].value).outcome
              == BattleAttributeModifierApplyOutcome::Refreshed);
        CHECK(state.effectCommands.attributeModifiers[0].amount == -30);
        CHECK(state.effectCommands.attributeModifiers[0].expiresFrameExclusive == 130);

        state = makeState();
        action.stackScope = EffectStackScope::EventSource;
        system.reduce(
            state,
            EffectCommand{ firstMetadata, ModifyAttributeEffectCommand{ action, -20 } },
            { .frame = 10 });
        system.reduce(
            state,
            EffectCommand{ secondMetadata, ModifyAttributeEffectCommand{ action, -30 } },
            { .frame = 40 });

        REQUIRE(state.effectCommands.attributeModifiers.size() == 2);
        CHECK(state.effectCommands.attributeModifiers[0].eventSourceUnitId == 1);
        CHECK(state.effectCommands.attributeModifiers[1].eventSourceUnitId == 2);
    }

    SECTION("傷害修正依效果來源刷新，事件來源範圍仍各自獨立")
    {
        auto state = makeState();
        auto first = damageModifierCommand(
            3,
            DamageModifierOperation::PercentAdd,
            -20,
            90,
            EffectStackPolicy::Refresh);
        first.metadata.eventSourceUnitId = 1;
        auto second = damageModifierCommand(
            3,
            DamageModifierOperation::PercentAdd,
            -30,
            90,
            EffectStackPolicy::Refresh);
        second.metadata.binding.ownerUnitId = 2;
        second.metadata.binding.runtimeInstanceId = 19;
        second.metadata.eventSourceUnitId = 2;

        BattleEffectCommandSystem system;
        system.reduce(state, first, { .frame = 10 });
        const auto refreshed = system.reduce(state, second, { .frame = 40 });

        REQUIRE(state.effectCommands.damageModifiers.size() == 1);
        CHECK(std::get<BattleDamageModifierEffectResult>(
                  refreshed.entries[0].value).outcome
              == BattleDamageModifierApplyOutcome::Refreshed);
        CHECK(state.effectCommands.damageModifiers[0].amount == -30);
        CHECK(state.effectCommands.damageModifiers[0].expiresFrameExclusive == 130);

        state = makeState();
        std::get<ModifyDamageEffectCommand>(first.value).action.stackScope =
            EffectStackScope::EventSource;
        std::get<ModifyDamageEffectCommand>(second.value).action.stackScope =
            EffectStackScope::EventSource;
        system.reduce(state, first, { .frame = 10 });
        system.reduce(state, second, { .frame = 40 });

        REQUIRE(state.effectCommands.damageModifiers.size() == 2);
        CHECK(state.effectCommands.damageModifiers[0].eventSourceUnitId == 1);
        CHECK(state.effectCommands.damageModifiers[1].eventSourceUnitId == 2);
    }
}

TEST_CASE("BattleEffectCommandSystem checks full MP healing after adjusted recovery", "[battle][effect][command][resource]")
{
    for (const int initialMp : { 70, 79, 80, 95, 100 })
    {
        for (const int bonus : { 0, 50 })
        {
            CAPTURE(initialMp, bonus);
            auto state = makeState();
            state.movement.frame = 10;
            state.units.requireCore(2).vitals.mp = initialMp;
            ModifyAttributeAction modifier;
            modifier.attribute = BattleAttribute::MpRecoveryBonus;
            modifier.operation = AttributeOperation::PercentagePointAdd;
            modifier.durationFrames = 100;
            BattleEffectCommandSystem system;
            system.reduce(state, EffectCommand{
                metadata(133, 2), ModifyAttributeEffectCommand{ modifier, bonus }
            }, { .frame = 10 });
            auto heal = resourceCommand(133, 2, BattleResource::Hp,
                ResourceChangeKind::Restore, 99, 1);
            std::get<ChangeResourceEffectCommand>(heal.value).action.healRequiresFullMp = true;
            const std::array commands{
                resourceCommand(133, 2, BattleResource::Mp, ResourceChangeKind::Restore, 20),
                heal,
            };
            system.reduce(state, commands, { .frame = 10 });
            const int expectedMp = std::min(100, initialMp + 20 * (100 + bonus) / 100);
            CHECK(state.units.requireCore(2).vitals.mp == expectedMp);
            CHECK(state.units.requireCore(2).vitals.hp == (expectedMp == 100 ? 299 : 200));
            CHECK(state.units.requireCore(2).shield == 0);
        }
    }
}

TEST_CASE("BattleEffectCommandSystem preserves resource transfer and drain semantics", "[battle][effect][command][resource]")
{
    SECTION("轉移只交付實際奪取量，不套用內力回復加成")
    {
        auto state = makeState();
        state.movement.frame = 10;
        ModifyAttributeAction recoveryBonus;
        recoveryBonus.attribute = BattleAttribute::MpRecoveryBonus;
        recoveryBonus.operation = AttributeOperation::PercentagePointAdd;
        recoveryBonus.durationFrames = 100;
        BattleEffectCommandSystem().reduce(
            state,
            EffectCommand{
                metadata(72, 2),
                ModifyAttributeEffectCommand{ recoveryBonus, 50 },
            },
            { .frame = 10 });

        auto command = resourceCommand(
            72,
            3,
            BattleResource::Mp,
            ResourceChangeKind::Transfer,
            24);
        auto& transfer = std::get<ChangeResourceEffectCommand>(command.value);
        transfer.transferDestinationUnitIds.push_back(2);

        const auto reduced = BattleEffectCommandSystem().reduce(
            state,
            command,
            { .frame = 10 });

        const auto& resource = std::get<BattleResourceEffectResult>(
            reduced.entries[0].value);
        REQUIRE(resource.deltas.size() == 2);
        CHECK(resource.deltas[0].unitId == 3);
        CHECK(resource.deltas[0].before == 80);
        CHECK(resource.deltas[0].after == 56);
        CHECK(resource.deltas[1].unitId == 2);
        CHECK(resource.deltas[1].before == 10);
        CHECK(resource.deltas[1].after == 34);
    }

    SECTION("吸取沿用既有語意，實際吸取量會套用內力回復加成")
    {
        auto state = makeState();
        state.movement.frame = 10;
        ModifyAttributeAction recoveryBonus;
        recoveryBonus.attribute = BattleAttribute::MpRecoveryBonus;
        recoveryBonus.operation = AttributeOperation::PercentagePointAdd;
        recoveryBonus.durationFrames = 100;
        BattleEffectCommandSystem().reduce(
            state,
            EffectCommand{
                metadata(28, 1),
                ModifyAttributeEffectCommand{ recoveryBonus, 50 },
            },
            { .frame = 10 });

        const auto command = resourceCommand(
            28,
            3,
            BattleResource::Mp,
            ResourceChangeKind::Drain,
            24);
        const auto reduced = BattleEffectCommandSystem().reduce(
            state,
            command,
            { .frame = 10 });

        const auto& resource = std::get<BattleResourceEffectResult>(
            reduced.entries[0].value);
        REQUIRE(resource.deltas.size() == 2);
        CHECK(resource.deltas[0].unitId == 3);
        CHECK(resource.deltas[0].before == 80);
        CHECK(resource.deltas[0].after == 56);
        CHECK(resource.deltas[1].unitId == 1);
        CHECK(resource.deltas[1].before == 20);
        CHECK(resource.deltas[1].after == 56);
    }
}

TEST_CASE("BattleEffectCommandSystem chains anti-combo attributes from their canonical basis", "[battle][effect][command][initialization][anti_combo]")
{
    BattleEffectCommandRuntimeState runtime;
    runtime.antiComboAttributeBases.emplace(
        BattleAntiComboAttributeKey{ 1, BattleAttribute::Attack },
        BattleAntiComboAttributeBasis{ 101, 0, 10 });
    runtime.antiComboAttributeBases.emplace(
        BattleAntiComboAttributeKey{ 2, BattleAttribute::Attack },
        BattleAntiComboAttributeBasis{ 101, 0, 10 });
    runtime.antiComboAttributeBases.emplace(
        BattleAntiComboAttributeKey{ 3, BattleAttribute::Attack },
        BattleAntiComboAttributeBasis{ 101, 0, 0 });

    auto initialized = metadata(33, 1);
    initialized.binding.kind = EffectSourceKind::Combo;
    initialized.binding.sourceId = 33;
    initialized.binding.runtimeInstanceId = 19;
    initialized.ruleId = EffectRuleId{ 3301 };
    initialized.event = EffectEvent::BattleInitialized;
    initialized.eventSourceUnitId = 1;
    ModifyAttributeAction action;
    action.attribute = BattleAttribute::Attack;
    action.operation = AttributeOperation::PercentAdd;
    const ModifyAttributeEffectCommand command{ action, 10 };
    BattleEffectCommandSystem::recordAntiComboInitialization(
        runtime,
        initialized,
        command);

    const auto transferred = BattleEffectCommandSystem::transferAntiComboInitialization(
        runtime,
        1,
        2,
        0,
        33);

    REQUIRE(transferred.coreAttributeDeltas.size() == 1);
    CHECK(transferred.coreAttributeDeltas[0].attribute == BattleAttribute::Attack);
    CHECK(transferred.coreAttributeDeltas[0].delta == 10);
    const auto& targetBasis = runtime.antiComboAttributeBases.at(
        BattleAntiComboAttributeKey{ 2, BattleAttribute::Attack });
    CHECK(targetBasis.percentTotal == 20);
    CHECK(BattleEffectCommandSystem::antiComboAttributeValue(targetBasis) == 121);

    int targetLiveAttack = 118;
    targetLiveAttack += transferred.coreAttributeDeltas[0].delta;
    CHECK(targetLiveAttack == 128);

    const auto chained = BattleEffectCommandSystem::transferAntiComboInitialization(
        runtime,
        2,
        3,
        1,
        33);
    REQUIRE(chained.coreAttributeDeltas.size() == 1);
    CHECK(chained.coreAttributeDeltas[0].delta == 10);
    const auto& chainedBasis = runtime.antiComboAttributeBases.at(
        BattleAntiComboAttributeKey{ 3, BattleAttribute::Attack });
    CHECK(chainedBasis.percentTotal == 10);
    CHECK(BattleEffectCommandSystem::antiComboAttributeValue(chainedBasis) == 111);
    REQUIRE(runtime.antiComboInitializationRecords.size() == 3);
    CHECK(runtime.antiComboInitializationRecords[2].metadata.binding.ownerUnitId == 3);
    CHECK(runtime.antiComboInitializationRecords[2].metadata.binding.sourceTeam == 1);
    CHECK(runtime.antiComboInitializationRecords[2].metadata.binding.runtimeInstanceId == 0);
    CHECK(runtime.antiComboInitializationRecords[2].metadata.targetUnitId == 3);
    CHECK(runtime.antiComboInitializationRecords[2].metadata.eventSourceUnitId == 3);
}

TEST_CASE("BattleEffectCommandSystem transfers anti-combo initialized resources exactly once", "[battle][effect][command][initialization][anti_combo][resource]")
{
    auto state = makeState();
    auto initialized = metadata(33, 1);
    initialized.binding.kind = EffectSourceKind::Combo;
    initialized.binding.sourceId = 33;
    initialized.ruleId = EffectRuleId{ 3302 };
    initialized.event = EffectEvent::BattleInitialized;
    ChangeResourceAction action;
    action.resource = BattleResource::Shield;
    action.kind = ResourceChangeKind::Grant;
    const ChangeResourceEffectCommand command{ action, 75 };
    BattleEffectCommandSystem::recordAntiComboInitialization(
        state.effectCommands,
        initialized,
        command);

    const auto transferred = BattleEffectCommandSystem::transferAntiComboInitialization(
        state.effectCommands,
        1,
        2,
        0,
        33);

    REQUIRE(transferred.commands.size() == 1);
    BattleEffectCommandSystem().reduce(
        state,
        transferred.commands,
        { .frame = 0 });
    CHECK(state.units.requireCore(2).shield == 75);
    CHECK(state.effectCommands.antiComboInitializationRecords.size() == 2);
}

TEST_CASE("BattleEffectCommandSystem clones live attribute modifiers without anti-combo records", "[battle][effect][command][initialization][anti_combo][clone]")
{
    BattleEffectCommandRuntimeState runtime;
    std::uint64_t sourceNextNegativeEffectSequence = 1;
    runtime.antiComboAttributeBases.emplace(
        BattleAntiComboAttributeKey{ 1, BattleAttribute::Attack },
        BattleAntiComboAttributeBasis{ 100, 5, 10 });

    auto initialized = metadata(33, 1);
    initialized.binding.kind = EffectSourceKind::Combo;
    initialized.binding.sourceId = 33;
    initialized.binding.runtimeInstanceId = 23;
    initialized.ruleId = EffectRuleId{ 3303 };
    initialized.event = EffectEvent::BattleInitialized;
    initialized.actionOrder = 4;
    initialized.eventSourceUnitId = 1;
    ModifyAttributeAction action;
    action.attribute = BattleAttribute::CriticalChance;
    action.operation = AttributeOperation::PercentagePointAdd;
    action.stack = EffectStackPolicy::AddStack;
    action.stackLimit = 5;
    action.stackScope = EffectStackScope::EventSource;
    const ModifyAttributeEffectCommand command{ action, -7 };
    BattleEffectCommandSystem::recordAntiComboInitialization(
        runtime,
        initialized,
        command);
    BattleEffectCommandSystem::applyPersistentAttributeModifier(
        runtime,
        initialized,
        command,
        0,
        &sourceNextNegativeEffectSequence);
    BattleEffectCommandSystem::applyPersistentAttributeModifier(
        runtime,
        initialized,
        command,
        0,
        &sourceNextNegativeEffectSequence);
    REQUIRE(runtime.attributeModifiers.size() == 1);
    CHECK(runtime.attributeModifiers[0].stackCount == 2);

    auto untrackedMetadata = initialized;
    untrackedMetadata.ruleId = EffectRuleId{ 3398 };
    BattleEffectCommandSystem::applyPersistentAttributeModifier(
        runtime,
        untrackedMetadata,
        command,
        0,
        &sourceNextNegativeEffectSequence);
    REQUIRE(runtime.attributeModifiers.size() == 2);

    auto cloneNextNegativeEffectSequence = sourceNextNegativeEffectSequence;
    BattleEffectCommandSystem::inheritCloneEffectModifiers(
        runtime,
        1,
        4,
        7,
        cloneNextNegativeEffectSequence);
    REQUIRE(runtime.antiComboInitializationRecords.size() == 1);
    CHECK_FALSE(runtime.antiComboAttributeBases.contains(
        BattleAntiComboAttributeKey{ 4, BattleAttribute::Attack }));
    REQUIRE(runtime.attributeModifiers.size() == 4);
    const auto& clonedModifier = runtime.attributeModifiers[2];
    CHECK(clonedModifier.binding.ownerUnitId == 4);
    CHECK(clonedModifier.binding.sourceTeam == 7);
    CHECK(clonedModifier.binding.runtimeInstanceId == 0);
    CHECK(clonedModifier.targetUnitId == 4);
    CHECK(clonedModifier.eventSourceUnitId == 4);
    CHECK(clonedModifier.stackCount == 2);
    const auto& clonedUntrackedModifier = runtime.attributeModifiers[3];
    CHECK(clonedUntrackedModifier.ruleId == EffectRuleId{ 3398 });
    CHECK(clonedUntrackedModifier.binding.ownerUnitId == 4);
    CHECK(clonedUntrackedModifier.targetUnitId == 4);
    CHECK(std::ranges::count_if(
        runtime.attributeModifiers,
        [](const auto& modifier) { return modifier.targetUnitId == 4; }) == 2);
    CHECK(cloneNextNegativeEffectSequence == sourceNextNegativeEffectSequence);
    CHECK(clonedModifier.negativeEffectSequence == 1);
    CHECK(clonedUntrackedModifier.negativeEffectSequence == 2);

    auto laterCloneMetadata = initialized;
    laterCloneMetadata.binding.ownerUnitId = 4;
    laterCloneMetadata.targetUnitId = 4;
    laterCloneMetadata.ruleId = EffectRuleId{ 3400 };
    const auto laterClone = BattleEffectCommandSystem::applyPersistentAttributeModifier(
        runtime,
        laterCloneMetadata,
        command,
        1,
        &cloneNextNegativeEffectSequence);
    CHECK(laterClone.modifier.negativeEffectSequence == 3);
    CHECK(cloneNextNegativeEffectSequence == 4);
}

TEST_CASE("BattleStatusRuntimeUnit rewrites only cloned self-source references", "[battle][effect][command][initialization][clone][status]")
{
    const auto contribution = [](BattleStatusKind kind,
                                 int sourceUnitId,
                                 EffectSourceKind sourceKind,
                                 int sourceId,
                                 EffectRuleId ruleId,
                                 std::uint32_t actionOrder)
    {
        const EffectSourceBinding binding{
            .kind = sourceKind,
            .sourceId = sourceId,
            .ownerUnitId = sourceUnitId,
            .sourceTeam = 7,
            .runtimeInstanceId = 19,
        };
        return BattleStatusContribution{
            .kind = kind,
            .producer = StatusProducerKey{
                .binding = binding,
                .ruleId = ruleId,
                .actionOrder = actionOrder,
                .behaviorRuleOrder = 3,
                .behaviorActionOrder = 4,
            },
            .producerFamily = StatusProducerFamilyKey{
                .sourceKind = sourceKind,
                .sourceId = sourceId,
                .logicalOwnerUnitId = sourceUnitId,
                .ruleId = ruleId,
                .actionOrder = actionOrder,
                .behaviorRuleOrder = 3,
                .behaviorActionOrder = 4,
            },
            .sourceUnitId = sourceUnitId,
            .origin = BattleStatusEffectOrigin{
                .binding = binding,
                .ruleId = ruleId,
                .ruleOrder = 11,
            },
        };
    };
    BattleStatusRuntimeUnit status;
    status.effects.statuses = {
        contribution(BattleStatusKind::Poison, 1, EffectSourceKind::Magic, 101, { 1001 }, 5),
        contribution(BattleStatusKind::Bleed, 3, EffectSourceKind::Equipment, 202, { 2002 }, 6),
    };

    rewriteBattleStatusSourceUnitId(status, 1, 4);

    const auto& cloned = status.effects.statuses[0];
    REQUIRE(cloned.producer);
    REQUIRE(cloned.producerFamily);
    REQUIRE(cloned.origin);
    CHECK(cloned.sourceUnitId == 4);
    CHECK(cloned.producer->binding.ownerUnitId == 4);
    CHECK(cloned.producerFamily->logicalOwnerUnitId == 4);
    CHECK(cloned.origin->binding.ownerUnitId == 4);
    CHECK(cloned.producer->binding.kind == EffectSourceKind::Magic);
    CHECK(cloned.producer->binding.sourceId == 101);
    CHECK(cloned.producer->binding.runtimeInstanceId == 19);
    CHECK(cloned.producer->ruleId == EffectRuleId{ 1001 });
    CHECK(cloned.producer->actionOrder == 5);
    CHECK(cloned.producerFamily->sourceKind == EffectSourceKind::Magic);
    CHECK(cloned.producerFamily->sourceId == 101);
    CHECK(cloned.producerFamily->ruleId == EffectRuleId{ 1001 });
    CHECK(cloned.producerFamily->actionOrder == 5);
    CHECK(cloned.origin->binding.kind == EffectSourceKind::Magic);
    CHECK(cloned.origin->binding.sourceId == 101);
    CHECK(cloned.origin->ruleId == EffectRuleId{ 1001 });
    CHECK(cloned.origin->ruleOrder == 11);

    const auto& external = status.effects.statuses[1];
    REQUIRE(external.producer);
    REQUIRE(external.producerFamily);
    REQUIRE(external.origin);
    CHECK(external.sourceUnitId == 3);
    CHECK(external.producer->binding.ownerUnitId == 3);
    CHECK(external.producerFamily->logicalOwnerUnitId == 3);
    CHECK(external.origin->binding.ownerUnitId == 3);
    CHECK(external.producer->binding.kind == EffectSourceKind::Equipment);
    CHECK(external.producer->binding.sourceId == 202);
    CHECK(external.producer->ruleId == EffectRuleId{ 2002 });
    CHECK(external.producer->actionOrder == 6);
}

TEST_CASE("BattleEffectCommandSystem transfers anti-combo status commands and clones the complete damage baseline", "[battle][effect][command][initialization][anti_combo][clone][status][damage]")
{
    auto state = makeState();
    auto& runtime = state.effectCommands;
    auto initialized = metadata(33, 1);
    initialized.binding.kind = EffectSourceKind::Combo;
    initialized.binding.sourceId = 33;
    initialized.binding.runtimeInstanceId = 29;
    initialized.ruleId = EffectRuleId{ 3304 };
    initialized.event = EffectEvent::BattleInitialized;
    initialized.eventSourceUnitId = 1;

    ModifyDamageAction damageAction;
    damageAction.perspective = DamageModifierPerspective::Incoming;
    damageAction.stage = DamageModifierStage::Final;
    damageAction.channel = DamageChannel::All;
    damageAction.operation = DamageModifierOperation::PercentAdd;
    damageAction.stack = EffectStackPolicy::AddStack;
    damageAction.stackLimit = 4;
    damageAction.stackScope = EffectStackScope::EventSource;
    const ModifyDamageEffectCommand damageCommand{ damageAction, -8 };
    BattleEffectCommandSystem::recordAntiComboInitialization(
        runtime,
        initialized,
        damageCommand);
    BattleEffectCommandSystem::applyPersistentDamageModifier(
        runtime,
        initialized,
        damageCommand,
        0);
    BattleEffectCommandSystem::applyPersistentDamageModifier(
        runtime,
        initialized,
        damageCommand,
        0);
    REQUIRE(runtime.damageModifiers.size() == 1);
    CHECK(runtime.damageModifiers[0].stackCount == 2);

    auto untrackedMetadata = initialized;
    untrackedMetadata.ruleId = EffectRuleId{ 3399 };
    untrackedMetadata.actionOrder = 9;
    BattleEffectCommandSystem::applyPersistentDamageModifier(
        runtime,
        untrackedMetadata,
        damageCommand,
        0);
    REQUIRE(runtime.damageModifiers.size() == 2);

    initialized.actionOrder = 1;
    ChangeResourceAction statusShieldAction;
    statusShieldAction.resource = BattleResource::StatusShield;
    statusShieldAction.kind = ResourceChangeKind::RefreshToAtLeast;
    const ChangeResourceEffectCommand statusShieldCommand{ statusShieldAction, 120 };
    BattleEffectCommandSystem::recordAntiComboInitialization(
        runtime,
        initialized,
        statusShieldCommand);

    initialized.actionOrder = 2;
    ApplyStatusAction statusAction;
    statusAction.status = BattleStatusKind::BattleSpirit;
    statusAction.durationFrames = 180;
    statusAction.quantity = AddStatusLayers{ 2, 5 };
    statusAction.behavior = battleSpiritStatusBehavior(17, 9);
    const ApplyStatusEffectCommand statusCommand{ statusAction, std::nullopt };
    BattleEffectCommandSystem::recordAntiComboInitialization(
        runtime,
        initialized,
        statusCommand);

    auto externalMetadata = initialized;
    externalMetadata.binding.ownerUnitId = 3;
    externalMetadata.binding.sourceTeam = 1;
    externalMetadata.ruleId = EffectRuleId{ 3305 };
    externalMetadata.actionOrder = 0;
    ApplyStatusAction externalStatusAction;
    externalStatusAction.status = BattleStatusKind::TrueQi;
    externalStatusAction.quantity = AddStatusLayers{ 1, 1 };
    externalStatusAction.behavior = trueQiStatusBehavior(5);
    const ApplyStatusEffectCommand externalStatusCommand{
        externalStatusAction,
        std::nullopt,
    };
    BattleEffectCommandSystem::recordAntiComboInitialization(
        runtime,
        externalMetadata,
        externalStatusCommand);
    const auto transferred = BattleEffectCommandSystem::transferAntiComboInitialization(
        runtime,
        1,
        2,
        0,
        33);
    REQUIRE(runtime.damageModifiers.size() == 3);
    const auto& transferredDamage = runtime.damageModifiers[2];
    CHECK(transferredDamage.binding.ownerUnitId == 2);
    CHECK(transferredDamage.binding.sourceTeam == 0);
    CHECK(transferredDamage.binding.runtimeInstanceId == 0);
    CHECK(transferredDamage.targetUnitId == 2);
    CHECK(transferredDamage.eventSourceUnitId == 2);
    CHECK(transferredDamage.stackCount == 2);
    REQUIRE(transferred.commands.size() == 2);
    CHECK(std::holds_alternative<ChangeResourceEffectCommand>(
        transferred.commands[0].value));
    CHECK(std::holds_alternative<ApplyStatusEffectCommand>(
        transferred.commands[1].value));
    CHECK(transferred.commands[1].metadata.binding.ownerUnitId == 2);
    CHECK(transferred.commands[1].metadata.targetUnitId == 2);
    BattleEffectCommandSystem().reduce(
        state,
        transferred.commands,
        { .frame = 0 });
    CHECK(state.units.require(2).status.effects.statusShield == 120);
    REQUIRE(state.units.require(2).status.effects.statuses.size() == 1);
    CHECK(state.units.require(2).status.effects.statuses[0].sourceUnitId == 2);

    std::uint64_t cloneNextNegativeEffectSequence = 1;
    BattleEffectCommandSystem::inheritCloneEffectModifiers(
        runtime,
        1,
        4,
        7,
        cloneNextNegativeEffectSequence);
    REQUIRE(runtime.damageModifiers.size() == 5);
    const auto& clonedDamage = runtime.damageModifiers[3];
    CHECK(clonedDamage.binding.ownerUnitId == 4);
    CHECK(clonedDamage.binding.sourceTeam == 7);
    CHECK(clonedDamage.binding.runtimeInstanceId == 0);
    CHECK(clonedDamage.targetUnitId == 4);
    CHECK(clonedDamage.eventSourceUnitId == 4);
    CHECK(clonedDamage.stackCount == 2);
    const auto& clonedUntrackedDamage = runtime.damageModifiers[4];
    CHECK(clonedUntrackedDamage.ruleId == EffectRuleId{ 3399 });
    CHECK(clonedUntrackedDamage.binding.ownerUnitId == 4);
    CHECK(clonedUntrackedDamage.targetUnitId == 4);
    CHECK(std::ranges::count_if(
        runtime.damageModifiers,
        [](const auto& modifier) { return modifier.targetUnitId == 4; }) == 2);
    REQUIRE(runtime.antiComboInitializationRecords.size() == 7);
    CHECK(std::ranges::none_of(
        runtime.antiComboInitializationRecords,
        [](const BattleAntiComboInitializationRecord& effect)
        {
            return effect.metadata.targetUnitId == 4;
        }));
}

TEST_CASE("BattleEffectCommandSystem starts refreshes and accumulates damage absorption", "[battle][effect][command][absorption]")
{
    auto state = makeState();
    const auto command = damageAbsorptionCommand(1, 80, 3);

    const auto started = BattleEffectCommandSystem().reduce(
        state,
        command,
        { .frame = 10 });

    REQUIRE(started.entries.size() == 1);
    const auto& applied = std::get<BattleDamageAbsorptionEffectResult>(
        started.entries[0].value);
    CHECK(applied.outcome == BattleDamageAbsorptionApplyOutcome::Applied);
    CHECK(applied.absorption.appliedFrame == 10);
    CHECK(applied.absorption.expiresFrameExclusive == 90);
    const auto sequence = applied.absorption.sequence;
    const auto activeLayers = BattleEffectCommandSystem::queryDamageAbsorptions(
        state,
        1,
        89);
    REQUIRE(activeLayers.size() == 1);
    CHECK(activeLayers[0].sequence == sequence);
    CHECK(activeLayers[0].absorbedPct == 40);
    CHECK(BattleEffectCommandSystem::queryDamageAbsorptions(state, 1, 90).empty());

    const std::array receipts{
        BattleDamageAbsorptionReceipt{ sequence, 24 },
    };
    BattleEffectCommandSystem::accumulateDamageAbsorptions(state, receipts);
    REQUIRE(state.effectCommands.damageAbsorptions.size() == 1);
    CHECK(state.effectCommands.damageAbsorptions[0].accumulatedDamage == 24);
    CHECK(state.effectRules.stateValue(
        applied.absorption.binding,
        EffectStateSlot::AbsorbedDamage) == 24);

    const auto refreshed = BattleEffectCommandSystem().reduce(
        state,
        command,
        { .frame = 20 });
    const auto& refresh = std::get<BattleDamageAbsorptionEffectResult>(
        refreshed.entries[0].value);
    CHECK(refresh.outcome == BattleDamageAbsorptionApplyOutcome::Refreshed);
    CHECK(refresh.absorption.sequence == sequence);
    CHECK(refresh.absorption.accumulatedDamage == 0);
    CHECK(refresh.absorption.appliedFrame == 20);
    CHECK(refresh.absorption.expiresFrameExclusive == 100);
    CHECK(state.effectRules.stateValue(
        refresh.absorption.binding,
        EffectStateSlot::AbsorbedDamage) == 0);
}

TEST_CASE("BattleEffectCommandSystem keeps status-owned absorption generations distinct and attributed",
          "[battle][effect][command][absorption][status][provenance]")
{
    auto state = makeState();
    auto& effects = state.units.require(1).status.effects;
    effects.statuses = {
        {
            .kind = BattleStatusKind::TrueQi,
            .stacks = 1,
            .appliedSequence = 41,
        },
        {
            .kind = BattleStatusKind::TrueQi,
            .stacks = 1,
            .appliedSequence = 43,
        },
    };
    const BattleAttackProvenance triggeringAttack{
        .cast = {
            .rootCastId = BattleCastId{ 31 },
            .castId = BattleCastId{ 31 },
            .sourceUnitId = 1,
            .magicId = 97,
            .ultimate = true,
        },
        .attackId = BattleAttackId{ 32 },
        .rootAttack = true,
        .mainProjectile = true,
    };

    auto first = damageAbsorptionCommand(1, 80, 3);
    first.metadata.executionLane = EffectExecutionLane::StatusBehavior;
    first.metadata.authoredActionOrder = 7;
    first.metadata.statusContribution = EffectStatusContributionContext{
        .holderUnitId = 1,
        .sourceUnitId = 1,
        .kind = BattleStatusKind::TrueQi,
        .quantity = 1,
        .appliedSequence = 41,
        .producerRuleId = EffectRuleId{ 97 },
        .producerRuleOrder = 11,
        .producerActionOrder = 2,
        .behaviorRuleOrder = 5,
    };
    auto second = first;
    second.metadata.statusContribution->appliedSequence = 43;

    const BattleEffectCommandContext context{
        .frame = 10,
        .cast = triggeringAttack.cast,
        .attack = triggeringAttack,
    };
    BattleEffectCommandSystem system;
    system.reduce(state, first, context);
    system.reduce(state, second, context);

    REQUIRE(state.effectCommands.damageAbsorptions.size() == 2);
    const auto& firstAbsorption = state.effectCommands.damageAbsorptions[0];
    const auto& secondAbsorption = state.effectCommands.damageAbsorptions[1];
    CHECK(firstAbsorption.authoredActionOrder == 7);
    CHECK(firstAbsorption.statusContribution == first.metadata.statusContribution);
    CHECK(secondAbsorption.statusContribution == second.metadata.statusContribution);
    CHECK(firstAbsorption.statusContribution != secondAbsorption.statusContribution);
    REQUIRE(firstAbsorption.triggeringCast);
    REQUIRE(firstAbsorption.triggeringAttack);
    CHECK(firstAbsorption.triggeringCast->castId == BattleCastId{ 31 });
    CHECK(firstAbsorption.triggeringAttack->attackId == BattleAttackId{ 32 });
    CHECK(firstAbsorption.sequence != secondAbsorption.sequence);
}

TEST_CASE("BattleEffectCommandSystem drains absorption exactly once at expiry or source death", "[battle][effect][command][absorption]")
{
    auto expiryState = makeState();
    BattleEffectCommandSystem().reduce(
        expiryState,
        damageAbsorptionCommand(1),
        { .frame = 10 });

    CHECK(BattleEffectCommandSystem::removeExpiredDamageAbsorptions(
        expiryState,
        89).empty());
    const auto expired = BattleEffectCommandSystem::removeExpiredDamageAbsorptions(
        expiryState,
        90);
    REQUIRE(expired.size() == 1);
    CHECK(BattleEffectCommandSystem::removeExpiredDamageAbsorptions(
        expiryState,
        90).empty());
    CHECK(BattleEffectCommandSystem::removeDamageAbsorptionsForSourceDeath(
        expiryState,
        1).empty());

    auto deathState = makeState();
    BattleEffectCommandSystem().reduce(
        deathState,
        damageAbsorptionCommand(1),
        { .frame = 10 });
    const auto died = BattleEffectCommandSystem::removeDamageAbsorptionsForSourceDeath(
        deathState,
        1);
    REQUIRE(died.size() == 1);
    CHECK(BattleEffectCommandSystem::removeDamageAbsorptionsForSourceDeath(
        deathState,
        1).empty());
    CHECK(BattleEffectCommandSystem::removeExpiredDamageAbsorptions(
        deathState,
        90).empty());
}

TEST_CASE("BattleEffectCommandSystem emits an explicit damage queue request", "[battle][effect][command]")
{
    auto state = makeState();
    DealDamageAction action;
    action.kind = BattleDamageKind::Pure;
    action.area.kind = DamageAreaKind::SingleTarget;
    const EffectCommand command{
        metadata(15, 3),
        DealDamageEffectCommand{ action, 125 },
    };

    const auto reduced = BattleEffectCommandSystem().reduce(
        state,
        command,
        { .frame = 20 });

    REQUIRE(reduced.entries.size() == 1);
    const auto& output = std::get<BattleEffectDamageRequestOutput>(reduced.entries[0].value);
    CHECK(output.request.attackerUnitId == 1);
    CHECK(output.request.defenderUnitId == 3);
    CHECK(output.request.baseDamage == 125);
    CHECK(output.request.damageKind == BattleDamageKind::Pure);
    CHECK_FALSE(output.request.acceptedHit);
    CHECK_FALSE(output.request.ignoreDefense);
    CHECK_FALSE(output.request.preResolvedDamage);
    CHECK(output.request.triggersDefenseEffects);
    CHECK(state.units.requireCore(3).vitals.hp == 1000);
}

TEST_CASE("BattleEffectCommandSystem preserves pre-resolved damage without hurt invincibility",
          "[battle][effect][command][migration]")
{
    auto state = makeState();
    DealDamageAction action;
    action.kind = BattleDamageKind::Effect;
    action.appliesDamageModifiers = false;
    action.triggersHurtInvincibility = false;
    const EffectCommand command{
        metadata(15, 3),
        DealDamageEffectCommand{ action, 30 },
    };

    const auto reduced = BattleEffectCommandSystem().reduce(
        state,
        command,
        { .frame = 20 });

    REQUIRE(reduced.entries.size() == 1);
    const auto& output = std::get<BattleEffectDamageRequestOutput>(
        reduced.entries.front().value);
    CHECK(output.request.preResolvedDamage);
    CHECK_FALSE(output.request.triggersDefenseEffects);
}

TEST_CASE("BattleEffectCommandSystem carries typed execute damage through the damage transaction", "[battle][effect][command][execute]")
{
    auto state = makeState();
    auto& target = state.units.requireCore(3);
    target.vitals.hp = 500;
    target.shield = 1000;

    DealDamageAction action;
    action.kind = BattleDamageKind::Execute;
    action.area.kind = DamageAreaKind::SingleTarget;
    const EffectCommand command{
        metadata(67, 3),
        DealDamageEffectCommand{ action, target.vitals.maxHp },
    };

    const auto reduced = BattleEffectCommandSystem().reduce(
        state,
        command,
        { .frame = 20 });

    REQUIRE(reduced.entries.size() == 1);
    const auto& output = std::get<BattleEffectDamageRequestOutput>(reduced.entries[0].value);
    CHECK(output.request.damageKind == BattleDamageKind::Execute);
    CHECK(output.request.canExecute);
    CHECK(output.request.executeThresholdPct == 100);

    BattleDamageTransactionInput transaction;
    transaction.request = output.request;
    transaction.attacker = state.units.require(1).damageState(0);
    transaction.defender = state.units.require(3).damageState(0);
    const auto result = BattleDamageSystem().resolveTransaction(transaction);

    CHECK(result.executed);
    CHECK(result.killed);
    CHECK_FALSE(result.defender.alive);
    CHECK(result.defender.vitals.hp == 0);
}

TEST_CASE("BattleEffectCommandSystem consumes sourced status layers and preserves independent damage count", "[battle][effect][command][stacked-status]")
{
    auto state = makeState();
    auto& target = state.units.require(3);
    target.status.effects.statusShield = 100;
    target.status.effects.statuses.push_back({
        .kind = BattleStatusKind::SevenStarMark,
        .sourceUnitId = 1,
        .remainingFrames = 150,
        .stacks = 2,
        .appliedSequence = 1,
    });

    ConsumeStatusAction consume;
    consume.status = BattleStatusKind::SevenStarMark;
    consume.quantity = 1;
    consume.source = StatusSourceMatch::EffectOwner;
    ApplyStatusAction stun;
    stun.status = BattleStatusKind::Stun;
    stun.durationFrames = 30;
    stun.quantity = NoStatusQuantity{};
    stun.reapplication = StatusReapplicationPolicy::KeepLongerDuration;
    consume.whenDepleted = stun;
    const EffectCommand consumeCommand{
        metadata(39, 3),
        ConsumeStatusEffectCommand{
            .action = consume,
            .whenDepleted = ApplyStatusEffectCommand{ stun, std::nullopt },
        },
    };

    const auto first = BattleEffectCommandSystem().reduce(
        state,
        consumeCommand,
        { .frame = 20 });
    const auto& firstConsume = std::get<BattleStatusConsumeEffectResult>(
        first.entries[0].value);
    CHECK(firstConsume.status.consumed);
    CHECK(firstConsume.status.remainingStacks == 1);
    CHECK_FALSE(firstConsume.depletedStatus);
    CHECK(target.status.effects.statusShield == 100);

    const auto second = BattleEffectCommandSystem().reduce(
        state,
        consumeCommand,
        { .frame = 20 });
    const auto& secondConsume = std::get<BattleStatusConsumeEffectResult>(
        second.entries[0].value);
    CHECK(secondConsume.status.consumed);
    CHECK(secondConsume.status.remainingStacks == 0);
    REQUIRE(secondConsume.depletedStatus);
    CHECK(secondConsume.depletedStatus->outcome
          == BattleStatusApplyOutcome::BlockedByStatusShield);
    CHECK_FALSE(target.status.effects.has(BattleStatusKind::Stun));
    CHECK(target.status.effects.statusShield == 70);

    DealDamageAction damage;
    damage.kind = BattleDamageKind::Pure;
    const EffectCommand damageCommand{
        metadata(95, 3),
        DealDamageEffectCommand{ damage, 120, 3 },
    };
    const auto reducedDamage = BattleEffectCommandSystem().reduce(
        state,
        damageCommand,
        { .frame = 20 });
    const auto& output = std::get<BattleEffectDamageRequestOutput>(
        reducedDamage.entries[0].value);
    CHECK(output.request.baseDamage == 120);
    CHECK(output.transactionCount == 3);
}

TEST_CASE("BattleEffectCommandSystem keeps current-hit damage modifiers routed", "[battle][effect][command][damage-modifier]")
{
    constexpr std::array operations{
        DamageModifierOperation::FlatAdd,
        DamageModifierOperation::PercentAdd,
        DamageModifierOperation::Multiply,
        DamageModifierOperation::IgnoreDefensePercent,
        DamageModifierOperation::CapSingleHitAtMaxHpPercent,
    };

    auto state = makeState();
    std::vector<EffectCommand> commands;
    for (std::uint32_t i = 0; i < operations.size(); ++i)
    {
        commands.push_back(damageModifierCommand(
            3,
            operations[i],
            static_cast<int>(i + 10),
            0,
            EffectStackPolicy::Independent,
            i));
    }

    const auto reduced = BattleEffectCommandSystem().reduce(
        state,
        commands,
        { .frame = 20 });

    REQUIRE(reduced.entries.size() == operations.size());
    CHECK(state.effectCommands.damageModifiers.empty());
    for (std::size_t i = 0; i < operations.size(); ++i)
    {
        REQUIRE(std::holds_alternative<
            BattleRoutedEffectCommand<ModifyDamageEffectCommand>>(
                reduced.entries[i].value));
        const auto& routed = std::get<
            BattleRoutedEffectCommand<ModifyDamageEffectCommand>>(
                reduced.entries[i].value);
        CHECK(routed.command.action.operation == operations[i]);
        CHECK(routed.command.amount == static_cast<int>(i + 10));
    }
}

TEST_CASE("BattleEffectCommandSystem stores and queries persistent damage modifiers", "[battle][effect][command][damage-modifier]")
{
    auto state = makeState();
    std::vector<EffectCommand> commands;
    constexpr std::array operations{
        DamageModifierOperation::FlatAdd,
        DamageModifierOperation::PercentAdd,
        DamageModifierOperation::Multiply,
        DamageModifierOperation::IgnoreDefensePercent,
        DamageModifierOperation::CapSingleHitAtMaxHpPercent,
    };
    for (std::uint32_t i = 0; i < operations.size(); ++i)
    {
        commands.push_back(damageModifierCommand(
            3,
            operations[i],
            static_cast<int>(i + 20),
            90,
            EffectStackPolicy::Independent,
            i,
            i % 2 == 0 ? DamageChannel::All : DamageChannel::Skill,
            DamageModifierStage::Final));
    }

    const auto reduced = BattleEffectCommandSystem().reduce(
        state,
        commands,
        { .frame = 10 });

    REQUIRE(reduced.entries.size() == operations.size());
    REQUIRE(state.effectCommands.damageModifiers.size() == operations.size());
    for (std::size_t i = 0; i < operations.size(); ++i)
    {
        const auto& applied = std::get<BattleDamageModifierEffectResult>(
            reduced.entries[i].value);
        CHECK(applied.outcome == BattleDamageModifierApplyOutcome::Applied);
        CHECK(applied.modifier.sequence == i + 1);
        CHECK(applied.modifier.operation == operations[i]);
        CHECK(applied.modifier.expiresFrameExclusive == 100);
    }

    const auto skill = BattleEffectCommandSystem::queryDamageModifiers(state, {
        .unitId = 3,
        .channel = DamageChannel::Skill,
        .stage = DamageModifierStage::Final,
        .frame = 99,
    });
    REQUIRE(skill.size() == operations.size());
    for (std::size_t i = 0; i < skill.size(); ++i)
    {
        CHECK(skill[i].sequence == i + 1);
    }

    const auto dot = BattleEffectCommandSystem::queryDamageModifiers(state, {
        .unitId = 3,
        .channel = DamageChannel::Dot,
        .stage = DamageModifierStage::Final,
        .frame = 99,
    });
    REQUIRE(dot.size() == 3);
    CHECK(dot[0].operation == DamageModifierOperation::FlatAdd);
    CHECK(dot[1].operation == DamageModifierOperation::Multiply);
    CHECK(dot[2].operation == DamageModifierOperation::CapSingleHitAtMaxHpPercent);

    CHECK(BattleEffectCommandSystem::queryDamageModifiers(state, {
        .unitId = 3,
        .channel = DamageChannel::Skill,
        .stage = DamageModifierStage::Final,
        .frame = 100,
    }).empty());
    CHECK(BattleEffectCommandSystem::queryDamageModifiers(state, {
        .unitId = 1,
        .channel = DamageChannel::Skill,
        .stage = DamageModifierStage::Final,
        .frame = 99,
    }).empty());
}

TEST_CASE("BattleEffectCommandSystem routes persistent negative modifiers through status protection", "[battle][effect][command][status-shield][modifier]")
{
    SECTION("限時負面屬性只保留狀態護盾未吸收的持續時間")
    {
        auto state = makeState();
        state.units.require(3).status.effects.statusShield = 30;
        ModifyAttributeAction action;
        action.attribute = BattleAttribute::Defence;
        action.operation = AttributeOperation::PercentAdd;
        action.durationFrames = 90;
        action.stack = EffectStackPolicy::Refresh;
        const EffectCommand command{
            metadata(59, 3),
            ModifyAttributeEffectCommand{ action, -30 },
        };

        const auto reduced = BattleEffectCommandSystem().reduce(
            state,
            command,
            { .frame = 10 });

        const auto& applied = std::get<BattleAttributeEffectResult>(
            reduced.entries[0].value);
        CHECK(applied.outcome == BattleAttributeModifierApplyOutcome::Applied);
        CHECK(applied.requestedDurationFrames == 90);
        CHECK(applied.appliedDurationFrames == 60);
        CHECK(applied.statusShieldAbsorbed == 30);
        CHECK(state.units.require(3).status.effects.statusShield == 0);
        REQUIRE(state.effectCommands.attributeModifiers.size() == 1);
        CHECK(state.effectCommands.attributeModifiers[0].negative);
        CHECK(state.effectCommands.attributeModifiers[0].expiresFrameExclusive == 70);
    }

    SECTION("狀態護盾足額時阻止負面傷害修正")
    {
        auto state = makeState();
        state.units.require(3).status.effects.statusShield = 100;

        const auto reduced = BattleEffectCommandSystem().reduce(
            state,
            damageModifierCommand(
                3,
                DamageModifierOperation::PercentAdd,
                -45,
                70,
                EffectStackPolicy::Independent,
                0,
                DamageChannel::All),
            { .frame = 10 });

        const auto& blocked = std::get<BattleDamageModifierEffectResult>(
            reduced.entries[0].value);
        CHECK(blocked.outcome
              == BattleDamageModifierApplyOutcome::BlockedByStatusShield);
        CHECK(blocked.requestedDurationFrames == 70);
        CHECK(blocked.appliedDurationFrames == 0);
        CHECK(blocked.statusShieldAbsorbed == 70);
        CHECK(state.units.require(3).status.effects.statusShield == 30);
        CHECK(state.effectCommands.damageModifiers.empty());
    }

    SECTION("無持續時間的負面修正固定消耗五十點防護")
    {
        auto state = makeState();
        state.units.require(3).status.effects.statusShield = 49;

        const auto reduced = BattleEffectCommandSystem().reduce(
            state,
            damageModifierCommand(
                3,
                DamageModifierOperation::PercentAdd,
                -20,
                0,
                EffectStackPolicy::Refresh,
                0,
                DamageChannel::All),
            { .frame = 10 });

        const auto& applied = std::get<BattleDamageModifierEffectResult>(
            reduced.entries[0].value);
        CHECK(applied.outcome == BattleDamageModifierApplyOutcome::Applied);
        CHECK(applied.statusShieldAbsorbed == 49);
        CHECK(state.units.require(3).status.effects.statusShield == 0);
        REQUIRE(state.effectCommands.damageModifiers.size() == 1);
        CHECK_FALSE(state.effectCommands.damageModifiers[0].expiresFrameExclusive);

        state = makeState();
        state.units.require(3).status.effects.statusShield = 50;
        const auto blocked = BattleEffectCommandSystem().reduce(
            state,
            damageModifierCommand(
                3,
                DamageModifierOperation::PercentAdd,
                -20,
                0,
                EffectStackPolicy::Refresh,
                0,
                DamageChannel::All),
            { .frame = 10 });
        CHECK(std::get<BattleDamageModifierEffectResult>(
                  blocked.entries[0].value).outcome
              == BattleDamageModifierApplyOutcome::BlockedByStatusShield);
        CHECK(state.effectCommands.damageModifiers.empty());
        CHECK(state.units.require(3).status.effects.statusShield == 0);
    }
}

TEST_CASE("BattleEffectCommandSystem removes negative statuses and persistent modifiers as one domain", "[battle][effect][command][status][modifier][cleanse]")
{
    auto makeAttributeCommand = [](int amount, int durationFrames)
    {
        ModifyAttributeAction action;
        action.attribute = BattleAttribute::Defence;
        action.operation = AttributeOperation::PercentAdd;
        action.durationFrames = durationFrames;
        action.stack = EffectStackPolicy::Independent;
        return EffectCommand{
            metadata(59, 3),
            ModifyAttributeEffectCommand{ action, amount },
        };
    };

    SECTION("先天功式全清會移除慕容減傷並保留正面修正")
    {
        auto state = makeState();
        auto& target = state.units.require(3);
        target.status.effects.statuses = {
            {
                .kind = BattleStatusKind::Poison,
                .sourceUnitId = 1,
                .remainingFrames = 30,
                .stacks = 1,
                .appliedSequence = 1,
            },
        };
        BattleEffectCommandSystem system;
        system.reduce(state, makeAttributeCommand(-30, 90), { .frame = 0 });
        system.reduce(state, makeAttributeCommand(20, 120), { .frame = 0 });
        system.reduce(
            state,
            damageModifierCommand(
                3,
                DamageModifierOperation::PercentAdd,
                -45,
                70,
                EffectStackPolicy::Independent,
                0,
                DamageChannel::All),
            { .frame = 0 });

        RemoveStatusAction action;
        action.negativeOnly = true;
        const EffectCommand cleanse{
            metadata(100, 3),
            RemoveStatusEffectCommand{ action },
        };
        const auto reduced = system.reduce(state, cleanse, { .frame = 10 });

        const auto& removed = std::get<BattleStatusRemoveEffectResult>(
            reduced.entries[0].value);
        CHECK(removed.status.removedCount == 3);
        CHECK(removed.removedAttributeModifiers.size() == 1);
        CHECK(removed.removedDamageModifiers.size() == 1);
        CHECK_FALSE(state.units.require(3).status.effects.has(BattleStatusKind::Poison));
        REQUIRE(state.effectCommands.attributeModifiers.size() == 1);
        CHECK(state.effectCommands.attributeModifiers[0].amount == 20);
        CHECK(state.effectCommands.damageModifiers.empty());
    }

    SECTION("清除一個最長負面會跨狀態與修正比較剩餘時間")
    {
        auto state = makeState();
        auto& target = state.units.require(3);
        target.status.effects.statuses = {
            {
                .kind = BattleStatusKind::Poison,
                .sourceUnitId = 1,
                .remainingFrames = 30,
                .stacks = 1,
                .appliedSequence = 1,
            },
        };
        BattleEffectCommandSystem system;
        system.reduce(
            state,
            damageModifierCommand(
                3,
                DamageModifierOperation::PercentAdd,
                -45,
                70,
                EffectStackPolicy::Independent,
                0,
                DamageChannel::All),
            { .frame = 0 });

        RemoveStatusAction action;
        action.negativeOnly = true;
        action.count = 1;
        action.order = StatusRemovalOrder::LongestRemaining;
        const EffectCommand cleanse{
            metadata(69, 3),
            RemoveStatusEffectCommand{ action },
        };
        const auto reduced = system.reduce(state, cleanse, { .frame = 10 });

        const auto& removed = std::get<BattleStatusRemoveEffectResult>(
            reduced.entries[0].value);
        CHECK(removed.status.removedCount == 1);
        CHECK(removed.removedDamageModifiers.size() == 1);
        CHECK(state.effectCommands.damageModifiers.empty());
        CHECK(state.units.require(3).status.effects.remainingFrames(BattleStatusKind::Poison) == 30);
    }

    SECTION("有限數量清除以狀態群組計數而不是以貢獻計數")
    {
        auto state = makeState();
        auto& effects = state.units.require(3).status.effects;
        effects.statuses = {
            {
                .kind = BattleStatusKind::Poison,
                .sourceUnitId = 1,
                .remainingFrames = 90,
                .stacks = 1,
                .appliedSequence = 1,
            },
            {
                .kind = BattleStatusKind::ColdPoison,
                .sourceUnitId = 1,
                .remainingFrames = 30,
                .stacks = 1,
                .appliedSequence = 2,
            },
        };

        RemoveStatusAction action;
        action.negativeOnly = true;
        action.count = 2;
        action.order = StatusRemovalOrder::Oldest;
        const EffectCommand cleanse{
            metadata(70, 3),
            RemoveStatusEffectCommand{ action },
        };
        const auto reduced = BattleEffectCommandSystem{}.reduce(
            state, cleanse, { .frame = 0 });

        const auto& removed = std::get<BattleStatusRemoveEffectResult>(
            reduced.entries[0].value);
        CHECK(removed.status.removedCount == 2);
        CHECK(std::ranges::contains(
            removed.status.removedStatuses, BattleStatusKind::Poison));
        CHECK(std::ranges::contains(
            removed.status.removedStatuses, BattleStatusKind::ColdPoison));
        CHECK(effects.statuses.empty());
    }

    SECTION("來源篩選同時限制狀態貢獻與持續修正")
    {
        auto state = makeState();
        auto& effects = state.units.require(3).status.effects;
        effects.statuses = {
            {
                .kind = BattleStatusKind::Poison,
                .sourceUnitId = 1,
                .remainingFrames = 90,
                .stacks = 1,
                .appliedSequence = 1,
            },
            {
                .kind = BattleStatusKind::ColdPoison,
                .sourceUnitId = 2,
                .remainingFrames = 60,
                .stacks = 1,
                .appliedSequence = 2,
            },
        };

        auto ownedModifier = damageModifierCommand(
            3,
            DamageModifierOperation::PercentAdd,
            -20,
            90,
            EffectStackPolicy::Independent,
            0,
            DamageChannel::All);
        auto otherModifier = damageModifierCommand(
            3,
            DamageModifierOperation::PercentAdd,
            -30,
            90,
            EffectStackPolicy::Independent,
            1,
            DamageChannel::All);
        otherModifier.metadata.binding.ownerUnitId = 2;
        otherModifier.metadata.binding.runtimeInstanceId = 2;
        BattleEffectCommandSystem system;
        system.reduce(state, ownedModifier, { .frame = 0 });
        system.reduce(state, otherModifier, { .frame = 0 });

        RemoveStatusAction action;
        action.negativeOnly = true;
        action.source = StatusSourceMatch::EffectOwner;
        action.count = 2;
        action.order = StatusRemovalOrder::Oldest;
        const EffectCommand cleanse{
            metadata(71, 3),
            RemoveStatusEffectCommand{ action },
        };
        const auto reduced = system.reduce(state, cleanse, { .frame = 0 });

        const auto& removed = std::get<BattleStatusRemoveEffectResult>(
            reduced.entries[0].value);
        CHECK(removed.status.removedCount == 2);
        CHECK(removed.status.removedStatuses
            == std::vector{ BattleStatusKind::Poison });
        CHECK(removed.removedDamageModifiers.size() == 1);
        REQUIRE(effects.statuses.size() == 1);
        CHECK(effects.statuses.front().sourceUnitId == 2);
        REQUIRE(state.effectCommands.damageModifiers.size() == 1);
        CHECK(state.effectCommands.damageModifiers.front().binding.ownerUnitId == 2);
    }

    SECTION("最早清除依持有者共用時序選出先施加的持續修正")
    {
        auto state = makeState();
        BattleEffectCommandSystem system;
        system.reduce(
            state,
            damageModifierCommand(
                3,
                DamageModifierOperation::PercentAdd,
                -20,
                90,
                EffectStackPolicy::Independent,
                0,
                DamageChannel::All),
            { .frame = 0 });

        ApplyStatusAction poison;
        poison.status = BattleStatusKind::Poison;
        poison.durationFrames = 90;
        poison.quantity = SetStatusTriggerCharges{ 1 };
        poison.reapplication = StatusReapplicationPolicy::KeepHigherDamage;
        poison.behavior = poisonStatusBehavior(10);
        system.reduce(
            state,
            EffectCommand{
                metadata(72, 3),
                ApplyStatusEffectCommand{ poison, std::nullopt },
            },
            { .frame = 1 });

        REQUIRE(state.effectCommands.damageModifiers.size() == 1);
        REQUIRE(state.units.require(3).status.effects.statuses.size() == 1);
        CHECK(state.effectCommands.damageModifiers.front().sequence == 1);
        CHECK(state.units.require(3).status.effects.statuses.front().appliedSequence == 1);
        CHECK(state.effectCommands.damageModifiers.front().negativeEffectSequence == 1);
        CHECK(state.units.require(3).status.effects.statuses.front().negativeEffectSequence == 2);

        RemoveStatusAction action;
        action.negativeOnly = true;
        action.count = 1;
        action.order = StatusRemovalOrder::Oldest;
        const auto reduced = system.reduce(
            state,
            EffectCommand{
                metadata(73, 3),
                RemoveStatusEffectCommand{ action },
            },
            { .frame = 2 });

        const auto& removed = std::get<BattleStatusRemoveEffectResult>(
            reduced.entries[0].value);
        CHECK(removed.status.removedCount == 1);
        CHECK(removed.removedDamageModifiers.size() == 1);
        CHECK(state.effectCommands.damageModifiers.empty());
        CHECK(state.units.require(3).status.effects.has(BattleStatusKind::Poison));
    }

    SECTION("最新清除依共用時序選擇並以儲存序號移除精確修正")
    {
        auto state = makeState();
        BattleEffectCommandSystem system;

        ApplyStatusAction poison;
        poison.status = BattleStatusKind::Poison;
        poison.durationFrames = 90;
        poison.quantity = SetStatusTriggerCharges{ 1 };
        poison.reapplication = StatusReapplicationPolicy::KeepHigherDamage;
        poison.behavior = poisonStatusBehavior(10);
        system.reduce(
            state,
            EffectCommand{
                metadata(74, 3),
                ApplyStatusEffectCommand{ poison, std::nullopt },
            },
            { .frame = 0 });
        system.reduce(
            state,
            damageModifierCommand(
                3,
                DamageModifierOperation::PercentAdd,
                -20,
                90,
                EffectStackPolicy::Independent,
                0,
                DamageChannel::All),
            { .frame = 1 });

        REQUIRE(state.effectCommands.damageModifiers.size() == 1);
        REQUIRE(state.units.require(3).status.effects.statuses.size() == 1);
        CHECK(state.effectCommands.damageModifiers.front().sequence == 1);
        CHECK(state.units.require(3).status.effects.statuses.front().appliedSequence == 1);
        CHECK(state.units.require(3).status.effects.statuses.front().negativeEffectSequence == 1);
        CHECK(state.effectCommands.damageModifiers.front().negativeEffectSequence == 2);

        RemoveStatusAction action;
        action.negativeOnly = true;
        action.count = 1;
        action.order = StatusRemovalOrder::Newest;
        const auto reduced = system.reduce(
            state,
            EffectCommand{
                metadata(75, 3),
                RemoveStatusEffectCommand{ action },
            },
            { .frame = 2 });

        const auto& removed = std::get<BattleStatusRemoveEffectResult>(
            reduced.entries[0].value);
        CHECK(removed.status.removedCount == 1);
        REQUIRE(removed.removedDamageModifiers.size() == 1);
        CHECK(removed.removedDamageModifiers.front().sequence == 1);
        CHECK(removed.removedDamageModifiers.front().negativeEffectSequence == 2);
        CHECK(state.effectCommands.damageModifiers.empty());
        CHECK(state.units.require(3).status.effects.has(BattleStatusKind::Poison));
    }

    SECTION("既有計時負面狀態與後套用修正共用持有者時序")
    {
        auto state = makeState();
        auto& effects = state.units.require(3).status.effects;
        effects.setFrames(BattleStatusKind::Stun, 90, 90, 1);

        BattleEffectCommandSystem system;
        system.reduce(
            state,
            damageModifierCommand(
                3,
                DamageModifierOperation::PercentAdd,
                -20,
                90,
                EffectStackPolicy::Independent,
                0,
                DamageChannel::All),
            { .frame = 1 });

        REQUIRE(effects.statuses.size() == 1);
        REQUIRE(state.effectCommands.damageModifiers.size() == 1);
        CHECK(effects.statuses.front().negativeEffectSequence == 1);
        CHECK(state.effectCommands.damageModifiers.front().sequence == 1);
        CHECK(state.effectCommands.damageModifiers.front().negativeEffectSequence == 2);

        RemoveStatusAction action;
        action.negativeOnly = true;
        action.count = 1;
        action.order = StatusRemovalOrder::Newest;
        const auto reduced = system.reduce(
            state,
            EffectCommand{
                metadata(76, 3),
                RemoveStatusEffectCommand{ action },
            },
            { .frame = 2 });

        const auto& removed = std::get<BattleStatusRemoveEffectResult>(
            reduced.entries[0].value);
        CHECK(removed.status.removedCount == 1);
        REQUIRE(removed.removedDamageModifiers.size() == 1);
        CHECK(removed.removedDamageModifiers.front().sequence == 1);
        CHECK(removed.removedDamageModifiers.front().negativeEffectSequence == 2);
        CHECK(state.effectCommands.damageModifiers.empty());
        CHECK(effects.has(BattleStatusKind::Stun));
    }
}

TEST_CASE("BattleEffectCommandSystem applies deterministic damage modifier stack policies", "[battle][effect][command][damage-modifier]")
{
    SECTION("刷新沿用 instance 順序並更新值與期限")
    {
        auto state = makeState();
        BattleEffectCommandSystem system;
        const auto first = system.reduce(
            state,
            damageModifierCommand(
                3,
                DamageModifierOperation::PercentAdd,
                -20,
                90,
                EffectStackPolicy::Refresh),
            { .frame = 10 });
        const auto second = system.reduce(
            state,
            damageModifierCommand(
                3,
                DamageModifierOperation::PercentAdd,
                -30,
                90,
                EffectStackPolicy::Refresh),
            { .frame = 40 });

        CHECK(std::get<BattleDamageModifierEffectResult>(first.entries[0].value).outcome
              == BattleDamageModifierApplyOutcome::Applied);
        const auto& refreshed = std::get<BattleDamageModifierEffectResult>(
            second.entries[0].value);
        CHECK(refreshed.outcome == BattleDamageModifierApplyOutcome::Refreshed);
        CHECK(refreshed.modifier.sequence == 1);
        CHECK(refreshed.modifier.amount == -30);
        CHECK(refreshed.modifier.expiresFrameExclusive == 130);
        REQUIRE(state.effectCommands.damageModifiers.size() == 1);
    }

    SECTION("獨立、取代與增加層數各自保有明確順序")
    {
        auto state = makeState();
        BattleEffectCommandSystem system;
        system.reduce(state, damageModifierCommand(
            3, DamageModifierOperation::FlatAdd, 10, 60), { .frame = 0 });
        system.reduce(state, damageModifierCommand(
            3, DamageModifierOperation::FlatAdd, 20, 60), { .frame = 1 });
        REQUIRE(state.effectCommands.damageModifiers.size() == 2);
        CHECK(state.effectCommands.damageModifiers[0].sequence == 1);
        CHECK(state.effectCommands.damageModifiers[1].sequence == 2);

        state = makeState();
        system.reduce(state, damageModifierCommand(
            3, DamageModifierOperation::FlatAdd, 10, 60,
            EffectStackPolicy::Replace), { .frame = 0 });
        const auto replaced = system.reduce(state, damageModifierCommand(
            3, DamageModifierOperation::FlatAdd, 20, 60,
            EffectStackPolicy::Replace), { .frame = 1 });
        REQUIRE(state.effectCommands.damageModifiers.size() == 1);
        CHECK(std::get<BattleDamageModifierEffectResult>(replaced.entries[0].value).outcome
              == BattleDamageModifierApplyOutcome::Replaced);
        CHECK(state.effectCommands.damageModifiers[0].sequence == 2);

    }

    SECTION("刷新跨越正負邊界會配置、清除並重新配置負面時序")
    {
        auto state = makeState();
        BattleEffectCommandSystem system;
        const auto refresh = [&](int amount, int frame)
        {
            return system.reduce(
                state,
                damageModifierCommand(
                    3,
                    DamageModifierOperation::PercentAdd,
                    amount,
                    90,
                    EffectStackPolicy::Refresh),
                { .frame = frame });
        };

        refresh(20, 0);
        REQUIRE(state.effectCommands.damageModifiers.size() == 1);
        CHECK_FALSE(state.effectCommands.damageModifiers.front().negative);
        CHECK(state.effectCommands.damageModifiers.front().negativeEffectSequence == 0);
        CHECK(state.units.require(3).status.effects.nextNegativeEffectSequence == 1);

        refresh(-20, 1);
        CHECK(state.effectCommands.damageModifiers.front().negative);
        CHECK(state.effectCommands.damageModifiers.front().negativeEffectSequence == 1);
        CHECK(state.units.require(3).status.effects.nextNegativeEffectSequence == 2);

        refresh(10, 2);
        CHECK_FALSE(state.effectCommands.damageModifiers.front().negative);
        CHECK(state.effectCommands.damageModifiers.front().negativeEffectSequence == 0);
        CHECK(state.units.require(3).status.effects.nextNegativeEffectSequence == 2);

        refresh(-30, 3);
        CHECK(state.effectCommands.damageModifiers.front().negative);
        CHECK(state.effectCommands.damageModifiers.front().negativeEffectSequence == 2);
        CHECK(state.units.require(3).status.effects.nextNegativeEffectSequence == 3);
    }

    SECTION("保留最強正確處理乘算與承傷上限")
    {
        auto state = makeState();
        BattleEffectCommandSystem system;
        system.reduce(state, damageModifierCommand(
            3, DamageModifierOperation::Multiply, 120, 60,
            EffectStackPolicy::KeepStrongest), { .frame = 0 });
        const auto stronger = system.reduce(state, damageModifierCommand(
            3, DamageModifierOperation::Multiply, 150, 60,
            EffectStackPolicy::KeepStrongest), { .frame = 1 });
        CHECK(std::get<BattleDamageModifierEffectResult>(stronger.entries[0].value).outcome
              == BattleDamageModifierApplyOutcome::Replaced);
        CHECK(state.effectCommands.damageModifiers[0].amount == 150);

        state = makeState();
        system.reduce(state, damageModifierCommand(
            3, DamageModifierOperation::CapSingleHitAtMaxHpPercent, 30, 60,
            EffectStackPolicy::KeepStrongest), { .frame = 0 });
        system.reduce(state, damageModifierCommand(
            3, DamageModifierOperation::CapSingleHitAtMaxHpPercent, 15, 60,
            EffectStackPolicy::KeepStrongest), { .frame = 1 });
        CHECK(state.effectCommands.damageModifiers[0].amount == 15);
    }
}
