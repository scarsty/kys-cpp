#include "battle/BattleEffectAttackCastSystem.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>

using namespace KysChess;
using namespace KysChess::Battle;

namespace
{

BattleCastInput castInput()
{
    BattleCastInput input;
    input.config.minimumFacingNorm = 0.0001;
    input.unit.id = 1;
    input.unit.mp = 100;
    input.unit.maxMp = 100;
    input.targetUnitId = 2;
    input.targetPosition = { 100.0f, 0.0f, 0.0f };
    input.normalSkill.id = 10;
    input.ultimateSkill.id = 20;
    input.projectileSpreadTargets = {
        { 2, { 100.0f, 0.0f, 0.0f } },
        { 3, { 0.0f, 100.0f, 0.0f } },
        { 4, { -100.0f, 0.0f, 0.0f } },
    };
    return input;
}

BattleAttackSpawnRequest baseAttack()
{
    BattleAttackSpawnRequest request;
    request.provenance.rootAttack = true;
    request.provenance.mainProjectile = true;
    request.initial.attackSourceUnitId = 1;
    request.initial.skillId = 20;
    request.initial.totalFrame = 30;
    request.initial.preferredTargetUnitId = 2;
    request.initial.position = { 0.0f, 0.0f, 0.0f };
    request.initial.velocity = { 10.0f, 0.0f, 0.0f };
    return request;
}

EffectCommandMetadata metadata(
    std::uint32_t targetOrder = 0,
    std::uint64_t commandOrdinal = 0,
    std::uint32_t actionOrder = 0)
{
    return {
        .binding = {
            .kind = EffectSourceKind::Magic,
            .sourceId = 59,
            .ownerUnitId = 1,
            .sourceTeam = 0,
        },
        .ruleId = EffectRuleId{ 1 },
        .event = EffectEvent::CastPlanned,
        .ruleOrder = 0,
        .actionOrder = actionOrder,
        .targetOrder = targetOrder,
        .commandOrdinal = commandOrdinal,
        .targetUnitId = 2,
    };
}

EffectCommand attackCommand(
    ModifyAttackAction action,
    int targetUnitId = 2,
    std::uint32_t targetOrder = 0,
    std::uint64_t commandOrdinal = 0,
    std::optional<int> damageOverride = std::nullopt,
    std::optional<ResolvedEffectAttackSource> source = std::nullopt)
{
    EffectCommand command;
    command.metadata = metadata(targetOrder, commandOrdinal);
    command.metadata.targetUnitId = targetUnitId;
    command.value = ModifyAttackEffectCommand{
        std::move(action),
        damageOverride,
        source,
    };
    return command;
}

EffectCommand castCommand(ModifyCastAction action, std::optional<int> mpCost)
{
    EffectCommand command;
    command.metadata = metadata();
    command.value = ModifyCastEffectCommand{ std::move(action), mpCost };
    return command;
}

void checkVelocity(const Pointf& velocity, double x, double y)
{
    CHECK(velocity.x == Catch::Approx(x).margin(0.0001));
    CHECK(velocity.y == Catch::Approx(y).margin(0.0001));
}

}  // namespace

TEST_CASE("BattleEffectAttackCastSystem expands 五虎扇形 into one shared-hit group",
          "[battle][effect][attack_cast]")
{
    auto input = castInput();
    BattleCastResult cast;
    cast.attackSpawnRequests.push_back(baseAttack());

    ModifyAttackAction action;
    action.pattern.kind = AttackPatternKind::Fan;
    action.pattern.projectileCount = 5;
    action.pattern.spreadDegrees = 60;
    action.strengthPct = 60;
    action.through = true;
    action.mainProjectile = true;
    action.sameTargetHitLimit = 1;
    action.propagation = CastPropagationPolicy::SourceHitRulesOnly;
    const std::array commands{ attackCommand(action) };
    BattleEffectAttackApplyState state{ .nextSharedHitGroupId = 41 };

    const auto applied = BattleEffectAttackCastSystem().applyAttackCommands(
        input, cast, commands, state);

    REQUIRE(cast.attackSpawnRequests.size() == 5);
    REQUIRE(applied.effectivePattern);
    CHECK(applied.effectivePattern->kind == AttackPatternKind::Fan);
    CHECK(applied.effectivePattern->projectileCount == 5);
    CHECK(cast.attackPattern.kind == AttackPatternKind::Fan);
    CHECK(cast.attackPattern.projectileCount == 5);
    CHECK(state.nextSharedHitGroupId == 42);
    CHECK(applied.hitLimits.size() == 5);
    CHECK(applied.lifecycle.size() == 5);
    CHECK(applied.deferred.empty());
    for (const auto& request : cast.attackSpawnRequests)
    {
        CHECK(request.initial.strengthPct == 60);
        CHECK(request.initial.through);
        CHECK(request.provenance.mainProjectile);
        CHECK(request.provenance.sharedHitGroupId == 41);
    }
    for (const auto& directive : applied.lifecycle)
    {
        CHECK(directive.propagation == CastPropagationPolicy::SourceHitRulesOnly);
    }
    checkVelocity(cast.attackSpawnRequests[0].initial.velocity, 8.660254, -5.0);
    checkVelocity(cast.attackSpawnRequests[2].initial.velocity, 10.0, 0.0);
    checkVelocity(cast.attackSpawnRequests[4].initial.velocity, 8.660254, 5.0);
}

TEST_CASE("BattleEffectAttackCastSystem appends 三分與六脈側翼主彈",
          "[battle][effect][attack_cast]")
{
    auto input = castInput();

    const auto run = [&](int projectileCount)
    {
        BattleCastResult cast;
        cast.attackSpawnRequests.push_back(baseAttack());
        ModifyAttackAction action;
        action.pattern.kind = AttackPatternKind::Flanks;
        action.pattern.projectileCount = projectileCount;
        action.pattern.spreadDegrees = 30;
        action.mainProjectile = true;
        action.addToBaseAttack = true;
        action.propagation = CastPropagationPolicy::SourceHitRulesOnly;
        const std::array commands{ attackCommand(action) };
        BattleEffectAttackApplyState state;
        const auto applied = BattleEffectAttackCastSystem().applyAttackCommands(
            input, cast, commands, state);
        return std::pair{ std::move(cast), std::move(applied) };
    };

    SECTION("三分追加兩道")
    {
        const auto [cast, applied] = run(2);
        REQUIRE(cast.attackSpawnRequests.size() == 3);
        CHECK(applied.lifecycle.size() == 2);
        checkVelocity(cast.attackSpawnRequests[0].initial.velocity, 10.0, 0.0);
        checkVelocity(cast.attackSpawnRequests[1].initial.velocity, 9.659258, -2.58819);
        checkVelocity(cast.attackSpawnRequests[2].initial.velocity, 9.659258, 2.58819);
    }

    SECTION("六脈追加三道")
    {
        const auto [cast, applied] = run(3);
        REQUIRE(cast.attackSpawnRequests.size() == 4);
        CHECK(applied.lifecycle.size() == 3);
        for (std::size_t index = 1; index < cast.attackSpawnRequests.size(); ++index)
        {
            CHECK(cast.attackSpawnRequests[index].provenance.mainProjectile);
        }
        checkVelocity(cast.attackSpawnRequests[1].initial.velocity, 9.914449, -1.305262);
        checkVelocity(cast.attackSpawnRequests[2].initial.velocity, 9.914449, 1.305262);
        checkVelocity(cast.attackSpawnRequests[3].initial.velocity, 9.659258, -2.58819);
    }
}

TEST_CASE("BattleEffectAttackCastSystem schedules 太嶽同落點三段延遲",
          "[battle][effect][attack_cast]")
{
    auto input = castInput();
    BattleCastResult cast;
    cast.attackSpawnRequests.push_back(baseAttack());

    ModifyAttackAction action;
    action.pattern.kind = AttackPatternKind::SamePointSequence;
    action.pattern.projectileCount = 3;
    action.pattern.intervalFrames = 10;
    action.strengthPct = 50;
    action.targets = AttackTargetPolicy::SamePoint;
    action.addToBaseAttack = true;
    action.propagation = CastPropagationPolicy::SourceHitRulesOnly;
    const std::array commands{ attackCommand(action) };
    BattleEffectAttackApplyState state;

    const auto applied = BattleEffectAttackCastSystem().applyAttackCommands(
        input, cast, commands, state);

    REQUIRE(cast.attackSpawnRequests.size() == 4);
    CHECK(applied.targets.size() == 3);
    CHECK(cast.attackSpawnRequests[1].spawnDelayFrames == 10);
    CHECK(cast.attackSpawnRequests[2].spawnDelayFrames == 20);
    CHECK(cast.attackSpawnRequests[3].spawnDelayFrames == 30);
    for (std::size_t index = 1; index < cast.attackSpawnRequests.size(); ++index)
    {
        const auto& request = cast.attackSpawnRequests[index];
        CHECK(request.initial.strengthPct == 50);
        CHECK(request.initial.preferredTargetUnitId == -1);
        CHECK_FALSE(request.initial.requirePreferredTarget);
        checkVelocity(request.initial.velocity, 10.0, 0.0);
    }
}

TEST_CASE("BattleEffectAttackCastSystem resolves selected multi-target and echo attacks deterministically",
          "[battle][effect][attack_cast]")
{
    auto input = castInput();

    SECTION("多目標不重複且依 selector targetOrder")
    {
        std::vector<BattleAttackSpawnRequest> requests{ baseAttack() };
        ModifyAttackAction action;
        action.pattern.kind = AttackPatternKind::MultiTarget;
        action.pattern.projectileCount = 6;
        action.strengthPct = 60;
        action.targets = AttackTargetPolicy::SelectedTargets;
        action.propagation = CastPropagationPolicy::SourceHitRulesOnly;
        std::array commands{
            attackCommand(action, 4, 2, 2),
            attackCommand(action, 2, 0, 0),
            attackCommand(action, 3, 1, 1),
        };
        BattleEffectAttackApplyState state;
        const auto applied = BattleEffectAttackCastSystem().applyAttackCommands(
            input, requests, commands, state);

        REQUIRE(requests.size() == 3);
        REQUIRE(applied.targets.size() == 3);
        CHECK(requests[0].initial.preferredTargetUnitId == 2);
        CHECK(requests[1].initial.preferredTargetUnitId == 3);
        CHECK(requests[2].initial.preferredTargetUnitId == 4);
        checkVelocity(requests[0].initial.velocity, 10.0, 0.0);
        checkVelocity(requests[1].initial.velocity, 0.0, 10.0);
        checkVelocity(requests[2].initial.velocity, -10.0, 0.0);
    }

    SECTION("殘影追加至基礎攻擊並標示 Echo provenance")
    {
        std::vector<BattleAttackSpawnRequest> requests{ baseAttack() };
        requests[0].initial.preferredTargetUnitId = -1;
        ModifyAttackAction action;
        action.pattern.kind = AttackPatternKind::EchoNearestOthers;
        action.pattern.projectileCount = 2;
        action.strengthPct = 50;
        action.mainProjectile = false;
        action.targets = AttackTargetPolicy::SelectedTargets;
        action.addToBaseAttack = true;
        action.propagation = CastPropagationPolicy::NoEffectRules;
        std::array commands{
            attackCommand(action, 4, 2, 2),
            attackCommand(action, 2, 0, 0),
            attackCommand(action, 3, 1, 1),
        };
        BattleEffectAttackApplyState state;
        const auto applied = BattleEffectAttackCastSystem().applyAttackCommands(
            input, requests, commands, state);

        REQUIRE(requests.size() == 3);
        REQUIRE(applied.lifecycle.size() == 2);
        CHECK(requests[0].provenance.mainProjectile);
        CHECK_FALSE(requests[1].provenance.mainProjectile);
        CHECK_FALSE(requests[2].provenance.mainProjectile);
        CHECK(requests[1].initial.preferredTargetUnitId == 3);
        CHECK(requests[2].initial.preferredTargetUnitId == 4);
        REQUIRE(applied.targets.size() == 2);
        CHECK(applied.targets[0].metadata.targetUnitId == 3);
        CHECK(applied.targets[1].metadata.targetUnitId == 4);
        for (const auto& directive : applied.lifecycle)
        {
            CHECK(directive.origin == BattleAttackOriginKind::Echo);
            CHECK(directive.propagation == CastPropagationPolicy::NoEffectRules);
        }
    }
}

TEST_CASE("BattleEffectAttackCastSystem applies preserve same-target damage overrides and exposes Core work",
          "[battle][effect][attack_cast]")
{
    auto input = castInput();
    std::vector<BattleAttackSpawnRequest> requests{ baseAttack() };
    ModifyAttackAction action;
    action.pattern.kind = AttackPatternKind::Preserve;
    action.pattern.projectileCount = 1;
    action.strengthPct = 80;
    action.tracking = true;
    action.mainProjectile = false;
    action.sameTargetHitLimit = 2;
    action.targets = AttackTargetPolicy::SameTarget;
    action.propagation = CastPropagationPolicy::SuppressUltimateRules;
    action.addToBaseAttack = true;
    action.damageOverride = EffectNumber{ .flat = 800 };
    action.damageKind = BattleDamageKind::Pure;
    const std::array commands{ attackCommand(action, 4, 0, 0, 800) };
    BattleEffectAttackApplyState state;

    const auto applied = BattleEffectAttackCastSystem().applyAttackCommands(
        input, requests, commands, state);

    REQUIRE(requests.size() == 2);
    const auto& extra = requests[1];
    CHECK(extra.initial.preferredTargetUnitId == 4);
    CHECK(extra.initial.requirePreferredTarget);
    CHECK(extra.initial.track);
    CHECK_FALSE(extra.provenance.mainProjectile);
    CHECK(extra.initial.strengthPct == 80);
    CHECK(extra.initial.scriptedDamage == 800);
    checkVelocity(extra.initial.velocity, -10.0, 0.0);
    REQUIRE(applied.damage.size() == 1);
    CHECK(applied.damage[0].damageKind == BattleDamageKind::Pure);
    CHECK(applied.damage[0].overrideStoredInScriptedDamage);
    REQUIRE(applied.hitLimits.size() == 1);
    CHECK_FALSE(applied.hitLimits[0].sharedHitGroupId);
    CHECK(std::ranges::any_of(applied.deferred, [](const auto& deferred)
    {
        return deferred.reason
            == BattleEffectDeferredAttackReason::SameTargetHitLimitNeedsRuntimeTracking;
    }));
}

TEST_CASE("BattleEffectAttackCastSystem rebases 夫妻刀法 projectile to its selected ally source",
          "[battle][effect][attack_cast][attack_source][couple_blade]")
{
    const auto input = castInput();
    auto prototype = baseAttack();
    BattleAttackProvenance sourceAttackProvenance;
    sourceAttackProvenance.cast = {
        .rootCastId = BattleCastId{ 21 },
        .castId = BattleCastId{ 21 },
        .sourceUnitId = 1,
        .magicId = 62,
        .ultimate = true,
        .origin = CastOriginKind::Ultimate,
    };
    sourceAttackProvenance.attackId = BattleAttackId{ 34 };
    sourceAttackProvenance.origin = BattleAttackOriginKind::Initial;
    sourceAttackProvenance.rootAttack = true;
    prototype.provenance.cast = sourceAttackProvenance.cast;
    prototype.provenance.origin = sourceAttackProvenance.origin;
    prototype.castWork = {
        .id = BattleCastWorkId{ 55 },
        .castId = BattleCastId{ 21 },
    };
    prototype.initial.through = true;
    prototype.initial.track = true;
    std::vector<BattleAttackSpawnRequest> requests{ prototype };

    ModifyAttackAction action;
    action.pattern.kind = AttackPatternKind::Preserve;
    action.pattern.projectileCount = 1;
    action.strengthPct = 100;
    action.mainProjectile = true;
    action.targets = AttackTargetPolicy::SameTarget;
    action.propagation = CastPropagationPolicy::SuppressUltimateRules;
    action.addToBaseAttack = true;
    const std::array commands{
        attackCommand(
            action,
            2,
            0,
            0,
            std::nullopt,
            ResolvedEffectAttackSource{
                .unitId = 7,
                .position = { 20.0f, 40.0f, 0.0f },
            }),
    };
    BattleEffectAttackApplyState state{
        .sourceAttackProvenance = sourceAttackProvenance,
    };

    const auto applied = BattleEffectAttackCastSystem().applyAttackCommands(
        input,
        requests,
        commands,
        state);

    REQUIRE(requests.size() == 2);
    CHECK(requests[0].provenance.valid());
    CHECK(requests[0].castWork.valid());
    CHECK(requests[0].initial.attackSourceUnitId == 1);
    CHECK(requests[0].initial.position.x == 0.0f);
    CHECK(requests[0].initial.position.y == 0.0f);

    const auto& allyAttack = requests[1];
    CHECK(allyAttack.initial.attackSourceUnitId == 7);
    CHECK(allyAttack.initial.position.x == 20.0f);
    CHECK(allyAttack.initial.position.y == 40.0f);
    CHECK(allyAttack.initial.preferredTargetUnitId == 2);
    CHECK(allyAttack.initial.requirePreferredTarget);
    CHECK(allyAttack.initial.strengthPct == 100);
    CHECK_FALSE(allyAttack.provenance.valid());
    CHECK(allyAttack.provenance.attackOrdinal == 0);
    CHECK_FALSE(allyAttack.provenance.rootAttack);
    CHECK(allyAttack.provenance.mainProjectile);
    CHECK(allyAttack.provenance.origin == BattleAttackOriginKind::CastDerived);
    REQUIRE(allyAttack.provenance.parentAttackId);
    CHECK(*allyAttack.provenance.parentAttackId == BattleAttackId{ 34 });
    CHECK(allyAttack.provenance.propagation
        == CastPropagationPolicy::SuppressUltimateRules);
    CHECK_FALSE(allyAttack.castWork.valid());
    CHECK(allyAttack.initial.through);
    CHECK(allyAttack.initial.track);
    checkVelocity(allyAttack.initial.velocity, 8.944272, -4.472136);

    REQUIRE(applied.lifecycle.size() == 1);
    CHECK(applied.lifecycle[0].requestIndex == 1);
    CHECK(applied.lifecycle[0].reservationRequired);
    CHECK_FALSE(applied.lifecycle[0].rootAttack);
    REQUIRE(applied.lifecycle[0].parentAttackId);
    CHECK(*applied.lifecycle[0].parentAttackId == BattleAttackId{ 34 });
    CHECK(applied.lifecycle[0].metadata.binding.ownerUnitId
          == sourceAttackProvenance.cast.sourceUnitId);
    CHECK(applied.lifecycle[0].propagation
        == CastPropagationPolicy::SuppressUltimateRules);
}

TEST_CASE("BattleEffectAttackCastSystem prepares zero-cost ranged casts and reports child casts",
          "[battle][effect][attack_cast]")
{
    auto input = castInput();
    ModifyCastAction action;
    action.mpCost = EffectNumber{ .flat = 0 };
    action.rangeMode = CastRangeMode::Ranged;
    action.freeAdditionalCast = true;
    action.propagation = CastPropagationPolicy::SuppressUltimateRules;
    const std::array commands{ castCommand(action, 0) };

    BattleEffectAttackCastSystem system;
    const auto preparation = system.prepareCast(input, commands);

    REQUIRE(preparation.mpCost == 0);
    REQUIRE(preparation.rangeMode == CastRangeMode::Ranged);
    CHECK(input.ultimateSkill.forceRanged);
    CHECK(input.ultimateSkill.rangedStyle);
    REQUIRE(preparation.freeAdditionalCasts.size() == 1);
    CHECK(preparation.freeAdditionalCasts[0].propagation
        == CastPropagationPolicy::SuppressUltimateRules);

    BattleCastResult cast;
    cast.mpDelta = -100;
    cast.attackSpawnRequests.push_back(baseAttack());
    BattleEffectAttackApplyState state;
    const auto applied = system.applyPreparedCast(input, cast, preparation, state);
    CHECK(cast.mpDelta == 0);
    REQUIRE(applied.lifecycle.size() == 1);
    CHECK(applied.lifecycle[0].propagation
        == CastPropagationPolicy::SuppressUltimateRules);
}

TEST_CASE("BattleEffectAttackCastSystem replacement pattern preserves attack payload fields",
          "[battle][effect][attack_cast]")
{
    auto input = castInput();
    ModifyCastAction action;
    action.replacementPattern = AttackPattern{
        .kind = AttackPatternKind::Fan,
        .projectileCount = 3,
        .spreadDegrees = 30,
    };
    const std::array commands{ castCommand(action, std::nullopt) };
    BattleEffectAttackCastSystem system;
    const auto preparation = system.prepareCast(input, commands);

    BattleCastResult cast;
    auto base = baseAttack();
    base.initial.strengthPct = 77;
    base.initial.through = true;
    base.initial.track = true;
    base.provenance.cast = {
        .rootCastId = BattleCastId{ 71 },
        .castId = BattleCastId{ 71 },
        .sourceUnitId = 1,
        .magicId = 20,
        .ultimate = true,
        .origin = CastOriginKind::Ultimate,
    };
    base.provenance.propagation = CastPropagationPolicy::SourceHitRulesOnly;
    base.provenance.origin = BattleAttackOriginKind::FollowUp;
    base.provenance.parentAttackId = BattleAttackId{ 70 };
    base.provenance.attackOrdinal = 7;
    base.provenance.mainProjectile = false;
    base.provenance.sharedHitGroupId = 19;
    base.castWork = {
        .id = BattleCastWorkId{ 72 },
        .castId = BattleCastId{ 71 },
    };
    cast.attackSpawnRequests.push_back(base);
    BattleEffectAttackApplyState state;
    const auto applied = system.applyPreparedCast(input, cast, preparation, state);

    REQUIRE(cast.attackSpawnRequests.size() == 3);
    REQUIRE(applied.effectivePattern);
    CHECK(applied.effectivePattern->kind == AttackPatternKind::Fan);
    CHECK(applied.effectivePattern->projectileCount == 3);
    CHECK(cast.attackPattern.kind == AttackPatternKind::Fan);
    CHECK(cast.attackPattern.projectileCount == 3);
    CHECK_FALSE(applied.deferredReplacementPattern);
    for (std::size_t index = 0; index < cast.attackSpawnRequests.size(); ++index)
    {
        const auto& request = cast.attackSpawnRequests[index];
        CHECK(request.initial.strengthPct == 77);
        CHECK(request.initial.through);
        CHECK(request.initial.track);
        CHECK_FALSE(request.provenance.mainProjectile);
        CHECK(request.provenance.propagation
            == CastPropagationPolicy::SourceRules);
        CHECK(request.provenance.origin == BattleAttackOriginKind::FollowUp);
        REQUIRE(request.provenance.parentAttackId);
        CHECK(*request.provenance.parentAttackId == BattleAttackId{ 70 });
        CHECK(request.provenance.sharedHitGroupId == 19);
        if (index == 0)
        {
            CHECK(request.provenance.valid());
            CHECK(request.provenance.attackOrdinal == 7);
            CHECK(request.provenance.rootAttack);
            CHECK(request.castWork.valid());
        }
        else
        {
            CHECK_FALSE(request.provenance.valid());
            CHECK(request.provenance.attackOrdinal == 0);
            CHECK_FALSE(request.provenance.rootAttack);
            CHECK_FALSE(request.castWork.valid());
        }
    }
}
