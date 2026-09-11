#include "battle/BattleStatusSystem.h"
#include "battle/BattleHealSystem.h"
#include "battle/BattlePresentationVisuals.h"
#include "ChessBattleEffectTypes.h"
#include "BattleCoreTestHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <stdexcept>
#include <string_view>

using namespace KysChess;
using namespace KysChess::Battle;
using namespace KysChess::Battle::Test;
using namespace BattlePresentationTest;

namespace
{

BattleStatusApplyRequest layeredContribution(
    EffectSourceBinding binding,
    EffectRuleId ruleId,
    std::uint32_t actionOrder,
    int layers,
    int limit,
    int value)
{
    return {
        .kind = BattleStatusKind::TrueQi,
        .producer = StatusProducerKey{ binding, ruleId, actionOrder },
        .producerFamily = StatusProducerFamilyKey{
            binding.kind,
            binding.sourceId,
            binding.ownerUnitId,
            ruleId,
            actionOrder,
        },
        .behavior = trueQiStatusBehavior(value),
        .sourceUnitId = binding.ownerUnitId,
        .stacks = layers,
        .origin = BattleStatusEffectOrigin{ binding, ruleId, 4 },
        .stack = EffectStackPolicy::AddStack,
        .stackLimit = limit,
    };
}

BattleStatusApplyRequest catalogDebuff(
    BattleStatusKind kind,
    EffectSourceBinding binding,
    EffectRuleId ruleId,
    int quantity = 1,
    int durationFrames = 0,
    std::optional<int> neutralizeMpRecovery = std::nullopt)
{
    ApplyStatusAction action;
    action.status = kind;
    action.durationFrames = durationFrames;
    if (kind == BattleStatusKind::SevenStarMark)
        action.quantity = SetStatusMarks{ quantity };
    else if (kind == BattleStatusKind::NeutralizeForce
        || kind == BattleStatusKind::Blinded)
        action.quantity = SetStatusTriggerCharges{ quantity };
    else
        action.quantity = NoStatusQuantity{};
    if (neutralizeMpRecovery)
        action.neutralizeMpRecovery = EffectNumber{ .flat = *neutralizeMpRecovery };
    action.behavior = makeCatalogOwnedStatusBehavior(action);
    const auto lowered = lowerStatusQuantity(action);
    return {
        .kind = kind,
        .producer = StatusProducerKey{ binding, ruleId, 0 },
        .producerFamily = StatusProducerFamilyKey{
            binding.kind,
            binding.sourceId,
            binding.ownerUnitId,
            ruleId,
            0,
        },
        .behavior = action.behavior,
        .sourceUnitId = binding.ownerUnitId,
        .durationFrames = durationFrames,
        .stacks = lowered.stacks,
        .origin = BattleStatusEffectOrigin{ binding, ruleId, 4 },
        .stack = lowerStatusReapplication(action),
    };
}

BattleStatusApplyRequest sharedBleed(
    EffectSourceBinding binding,
    EffectRuleId ruleId,
    int layers,
    int targetTotalLimit)
{
    ApplyStatusAction action;
    action.status = BattleStatusKind::Bleed;
    action.quantity = AddSharedStatusLayers{ layers, targetTotalLimit };
    action.behavior = makeCatalogOwnedStatusBehavior(action);
    return {
        .kind = BattleStatusKind::Bleed,
        .producer = StatusProducerKey{ binding, ruleId, 0 },
        .producerFamily = StatusProducerFamilyKey{
            binding.kind,
            binding.sourceId,
            binding.ownerUnitId,
            ruleId,
            0,
        },
        .behavior = action.behavior,
        .sourceUnitId = binding.ownerUnitId,
        .stacks = layers,
        .origin = BattleStatusEffectOrigin{ binding, ruleId, 4 },
        .stack = EffectStackPolicy::AddStack,
        .targetTotalLimit = targetTotalLimit,
    };
}

BattleStatusUnitState statusTarget(int id)
{
    return {
        .id = id,
        .alive = true,
        .hp = 100,
        .maxHp = 100,
    };
}

BattleStatusContribution persistentContribution(
    BattleStatusKind kind,
    std::shared_ptr<const StatusBehaviorDefinition> behavior,
    EffectSourceKind sourceKind,
    int sourceId,
    std::uint32_t ruleOrder,
    int stacks,
    std::uint64_t appliedSequence)
{
    const EffectSourceBinding binding{
        .kind = sourceKind,
        .sourceId = sourceId,
        .ownerUnitId = 1,
        .sourceTeam = 0,
    };
    const EffectRuleId ruleId{ static_cast<std::uint64_t>(sourceId) };
    return {
        .kind = kind,
        .producer = StatusProducerKey{ binding, ruleId, 0 },
        .producerFamily = StatusProducerFamilyKey{
            binding.kind,
            binding.sourceId,
            binding.ownerUnitId,
            ruleId,
            0,
        },
        .behavior = std::move(behavior),
        .behaviorRuntime = { EffectRuleRuntimeState{} },
        .sourceUnitId = binding.ownerUnitId,
        .stacks = stacks,
        .origin = BattleStatusEffectOrigin{ binding, ruleId, ruleOrder },
        .appliedSequence = appliedSequence,
    };
}

}

TEST_CASE("BattleStatusGroupStorage materializes the catalog storage taxonomy per holder",
          "[battle][status][group][catalog][structure]")
{
    BattleStatusGroupStorage storage;
    storage.push_back({ .kind = BattleStatusKind::TrueQi, .appliedSequence = 1 });
    storage.push_back({ .kind = BattleStatusKind::Stun, .appliedSequence = 2 });
    storage.push_back({ .kind = BattleStatusKind::TrueQi, .appliedSequence = 3 });
    storage.push_back({ .kind = BattleStatusKind::Bleed, .appliedSequence = 4 });
    storage.push_back({ .kind = BattleStatusKind::WitheredBone, .appliedSequence = 5 });

    CHECK(storage.groupCount() == 4);
    const auto* trueQi = storage.groupState(BattleStatusKind::TrueQi);
    REQUIRE(trueQi);
    REQUIRE(std::holds_alternative<ProducerOwnedContributions>(*trueQi));
    CHECK(std::get<ProducerOwnedContributions>(*trueQi).contributionIndices
        == std::vector<std::size_t>{ 0, 2 });
    const auto* stun = storage.groupState(BattleStatusKind::Stun);
    REQUIRE(stun);
    CHECK(std::get<SharedDurationControl>(*stun).contributionIndex == 1);
    const auto* bleed = storage.groupState(BattleStatusKind::Bleed);
    REQUIRE(bleed);
    CHECK(std::get<SharedLayerDebuff>(*bleed).contributionIndex == 3);
    const auto* withered = storage.groupState(BattleStatusKind::WitheredBone);
    REQUIRE(withered);
    CHECK(std::get<SelectedDebuffInstance>(*withered).contributionIndex == 4);

    storage.erase(storage.begin());
    trueQi = storage.groupState(BattleStatusKind::TrueQi);
    REQUIRE(trueQi);
    CHECK(std::get<ProducerOwnedContributions>(*trueQi).contributionIndices
        == std::vector<std::size_t>{ 1 });

    // Taxonomy is derived at the query boundary, so an externally mutable
    // contribution identity cannot leave a stale True-Qi group behind.
    storage[1].kind = BattleStatusKind::BattleSpirit;
    CHECK(storage.groupState(BattleStatusKind::TrueQi) == nullptr);
    const auto* battleSpirit = storage.groupState(BattleStatusKind::BattleSpirit);
    REQUIRE(battleSpirit);
    CHECK(std::get<ProducerOwnedContributions>(*battleSpirit).contributionIndices
        == std::vector<std::size_t>{ 1 });
}

TEST_CASE("BattleStatusSystem keeps same-name producer contributions independent",
          "[battle][status][contribution]")
{
    const EffectSourceBinding jiuyang{
        .kind = EffectSourceKind::Magic,
        .sourceId = 106,
        .ownerUnitId = 1,
        .sourceTeam = 0,
    };
    const EffectSourceBinding other{
        .kind = EffectSourceKind::Magic,
        .sourceId = 900,
        .ownerUnitId = 1,
        .sourceTeam = 0,
    };
    BattleStatusSystem system({});
    auto target = statusTarget(1);
    target = system.apply(
        std::move(target),
        layeredContribution(jiuyang, EffectRuleId{ 10 }, 0, 10, 10, 9)).target;
    target = system.apply(
        std::move(target),
        layeredContribution(other, EffectRuleId{ 20 }, 0, 1, 5, 20)).target;

    REQUIRE(target.effects.statuses.size() == 2);
    CHECK(target.effects.statuses[0].stacks == 10);
    CHECK(std::get<DealDamageAction>(
        target.effects.statuses[0].behavior->rules.front().actions.front().value)
        .amount.flat == 9);
    CHECK(target.effects.statuses[1].stacks == 1);
    CHECK(std::get<DealDamageAction>(
        target.effects.statuses[1].behavior->rules.front().actions.front().value)
        .amount.flat == 20);
    const auto independent = system.snapshot(target);
    CHECK(independent.stacks(BattleStatusKind::TrueQi) == 11);
    CHECK(independent.familyCapacity(BattleStatusKind::TrueQi) == 15);

    target = system.apply(
        std::move(target),
        layeredContribution(other, EffectRuleId{ 20 }, 0, 8, 5, 20)).target;
    REQUIRE(target.effects.statuses.size() == 2);
    CHECK(target.effects.statuses[0].stacks == 10);
    CHECK(target.effects.statuses[1].stacks == 5);

    const auto jiuyangSequence = target.effects.statuses[0].appliedSequence;
    const auto otherSequence = target.effects.statuses[1].appliedSequence;
    const auto jiuyangOrigin = target.effects.statuses[0].origin;
    target = system.apply(
        std::move(target),
        layeredContribution(jiuyang, EffectRuleId{ 10 }, 0, 3, 10, 9)).target;
    REQUIRE(target.effects.statuses.size() == 2);
    CHECK(target.effects.statuses[0].stacks == 10);
    CHECK(target.effects.statuses[0].appliedSequence == jiuyangSequence);
    CHECK(target.effects.statuses[0].origin == jiuyangOrigin);
    CHECK(target.effects.statuses[1].stacks == 5);
    CHECK(target.effects.statuses[1].appliedSequence == otherSequence);

    auto consumedOther = system.consume(
        std::move(target),
        {
            .kind = BattleStatusKind::TrueQi,
            .stacks = 5,
            .filter = {
                .appliedSequence = otherSequence,
            },
        });
    REQUIRE(consumedOther.consumed);
    REQUIRE(consumedOther.target.effects.statuses.size() == 1);
    CHECK(consumedOther.target.effects.statuses.front().appliedSequence
        == jiuyangSequence);

    auto removedGroup = system.remove(
        std::move(consumedOther.target),
        { .statuses = { BattleStatusKind::TrueQi } });
    CHECK(removedGroup.removedCount == 1);
    CHECK_FALSE(removedGroup.target.effects.has(BattleStatusKind::TrueQi));
}

TEST_CASE("BattleStatusSystem filters status queries consumption and removal by exact provenance",
          "[battle][status][contribution][filter]")
{
    const EffectSourceBinding first{
        .kind = EffectSourceKind::Magic,
        .sourceId = 106,
        .ownerUnitId = 1,
        .sourceTeam = 0,
    };
    const EffectSourceBinding second{
        .kind = EffectSourceKind::Equipment,
        .sourceId = 901,
        .ownerUnitId = 1,
        .sourceTeam = 0,
    };
    const EffectSourceBinding third{
        .kind = EffectSourceKind::Combo,
        .sourceId = 77,
        .ownerUnitId = 2,
        .sourceTeam = 0,
    };
    BattleStatusSystem system({});
    auto target = statusTarget(5);
    target = system.apply(
        std::move(target),
        layeredContribution(first, EffectRuleId{ 10 }, 0, 3, 10, 9)).target;
    target = system.apply(
        std::move(target),
        layeredContribution(second, EffectRuleId{ 20 }, 0, 4, 10, 9)).target;
    target = system.apply(
        std::move(target),
        layeredContribution(third, EffectRuleId{ 30 }, 0, 5, 10, 9)).target;

    REQUIRE(target.effects.statuses.size() == 3);
    const auto firstSequence = target.effects.statuses[0].appliedSequence;
    const auto thirdSequence = target.effects.statuses[2].appliedSequence;
    const auto snapshot = system.snapshot(target);
    CHECK(snapshot.stacks(BattleStatusKind::TrueQi) == 12);
    CHECK(snapshot.stacks(
        BattleStatusKind::TrueQi,
        { .sourceUnitId = 1 }) == 7);
    CHECK(snapshot.stacks(
        BattleStatusKind::TrueQi,
        { .producerBinding = second }) == 4);
    CHECK(snapshot.stacks(
        BattleStatusKind::TrueQi,
        { .holderUnitId = 5, .appliedSequence = thirdSequence }) == 5);
    CHECK(snapshot.stacks(
        BattleStatusKind::TrueQi,
        { .holderUnitId = 6, .appliedSequence = thirdSequence }) == 0);

    const auto wrongHolderConsume = system.consume(
        target,
        {
            .kind = BattleStatusKind::TrueQi,
            .filter = {
                .holderUnitId = 6,
                .appliedSequence = firstSequence,
            },
        });
    CHECK_FALSE(wrongHolderConsume.consumed);
    CHECK(wrongHolderConsume.target.effects.statuses.size() == 3);

    const auto filteredRemoval = system.remove(
        target,
        {
            .statuses = { BattleStatusKind::TrueQi },
            .filter = { .producerBinding = second },
            .count = 1,
        });
    CHECK(filteredRemoval.removedCount == 1);
    REQUIRE(filteredRemoval.target.effects.statuses.size() == 2);
    CHECK(system.snapshot(filteredRemoval.target).stacks(
        BattleStatusKind::TrueQi) == 8);
    CHECK(system.snapshot(filteredRemoval.target).stacks(
        BattleStatusKind::TrueQi,
        { .producerBinding = first }) == 3);
    CHECK(system.snapshot(filteredRemoval.target).stacks(
        BattleStatusKind::TrueQi,
        { .producerBinding = third }) == 5);

    const auto unfilteredRemoval = system.remove(
        filteredRemoval.target,
        {
            .statuses = { BattleStatusKind::TrueQi },
            .count = 1,
        });
    CHECK(unfilteredRemoval.removedCount == 1);
    CHECK_FALSE(unfilteredRemoval.target.effects.has(BattleStatusKind::TrueQi));
}

TEST_CASE("BattleStatusSystem reports no group capacity when any live contribution is unbounded",
          "[battle][status][contribution][capacity][unbounded]")
{
    const EffectSourceBinding boundedSource{
        .kind = EffectSourceKind::Magic,
        .sourceId = 106,
        .ownerUnitId = 1,
    };
    BattleStatusSystem system({});
    auto target = system.apply(
        statusTarget(5),
        layeredContribution(
            boundedSource,
            EffectRuleId{ 10 },
            0,
            3,
            10,
            9)).target;
    target.effects.statuses.push_back({
        .kind = BattleStatusKind::TrueQi,
        .sourceUnitId = 2,
        .stacks = 4,
        .appliedSequence = target.effects.nextStatusSequence++,
    });

    const auto snapshot = system.snapshot(target);
    CHECK(snapshot.stacks(BattleStatusKind::TrueQi) == 7);
    CHECK_FALSE(snapshot.familyCapacity(BattleStatusKind::TrueQi));
}

TEST_CASE("Stun and MP block clocks keep the provenance of the application that owns the clock",
          "[battle][status][contribution][clock][filter]")
{
    const EffectSourceBinding first{
        .kind = EffectSourceKind::Magic,
        .sourceId = 101,
        .ownerUnitId = 1,
        .sourceTeam = 0,
    };
    const EffectSourceBinding second{
        .kind = EffectSourceKind::Equipment,
        .sourceId = 202,
        .ownerUnitId = 2,
        .sourceTeam = 0,
    };
    const auto request = [](
        BattleStatusKind kind,
        EffectSourceBinding binding,
        int durationFrames)
    {
        const EffectRuleId ruleId{
            static_cast<std::uint64_t>(binding.sourceId) };
        BattleStatusApplyRequest result;
        result.kind = kind;
        result.producer = StatusProducerKey{ binding, ruleId, 0 };
        result.producerFamily = StatusProducerFamilyKey{
            binding.kind,
            binding.sourceId,
            binding.ownerUnitId,
            ruleId,
            0,
        };
        result.sourceUnitId = binding.ownerUnitId;
        result.durationFrames = durationFrames;
        result.origin = BattleStatusEffectOrigin{ binding, ruleId, 7 };
        result.stack = EffectStackPolicy::Refresh;
        return result;
    };

    for (const auto kind : {
             BattleStatusKind::Stun,
             BattleStatusKind::MpBlocked,
         })
    {
        DYNAMIC_SECTION("longer incoming application owns "
                        << battleStatusLabel(kind))
        {
            BattleStatusSystem system({});
            auto target = system.apply(
                statusTarget(5), request(kind, first, 40)).target;
            target = system.apply(
                std::move(target), request(kind, second, 80)).target;

            REQUIRE(target.effects.statuses.size() == 1);
            const auto& representative = target.effects.statuses.front();
            REQUIRE(representative.producer);
            CHECK(representative.remainingFrames == 80);
            CHECK(representative.sourceUnitId == second.ownerUnitId);
            CHECK(representative.producer->binding == second);
            const auto snapshot = system.snapshot(target);
            CHECK(snapshot.stacks(kind, { .sourceUnitId = 2 }) == 1);
            CHECK(snapshot.stacks(kind, { .producerBinding = second }) == 1);
            CHECK(snapshot.stacks(kind, { .sourceUnitId = 1 }) == 0);
            CHECK(snapshot.stacks(kind, { .producerBinding = first }) == 0);
        }

        DYNAMIC_SECTION("shorter incoming application does not steal "
                        << battleStatusLabel(kind))
        {
            BattleStatusSystem system({});
            auto target = system.apply(
                statusTarget(5), request(kind, first, 80)).target;
            target = system.apply(
                std::move(target), request(kind, second, 40)).target;

            REQUIRE(target.effects.statuses.size() == 1);
            const auto& representative = target.effects.statuses.front();
            REQUIRE(representative.producer);
            CHECK(representative.remainingFrames == 80);
            CHECK(representative.sourceUnitId == first.ownerUnitId);
            CHECK(representative.producer->binding == first);
            const auto snapshot = system.snapshot(target);
            CHECK(snapshot.stacks(kind, { .sourceUnitId = 1 }) == 1);
            CHECK(snapshot.stacks(kind, { .producerBinding = first }) == 1);
            CHECK(snapshot.stacks(kind, { .sourceUnitId = 2 }) == 0);
            CHECK(snapshot.stacks(kind, { .producerBinding = second }) == 0);
        }
    }
}

TEST_CASE("BattleStatusSystem presents heterogeneous status families without a global cap",
          "[battle][status][contribution][presentation]")
{
    const EffectSourceBinding jiuyang{
        .kind = EffectSourceKind::Magic,
        .sourceId = 106,
        .ownerUnitId = 1,
        .sourceTeam = 0,
    };
    const EffectSourceBinding other{
        .kind = EffectSourceKind::Magic,
        .sourceId = 900,
        .ownerUnitId = 1,
        .sourceTeam = 0,
    };
    BattleStatusSystem system({});
    auto target = system.apply(
        statusTarget(5),
        layeredContribution(jiuyang, EffectRuleId{ 10 }, 0, 7, 10, 9)).target;
    target = system.apply(
        std::move(target),
        layeredContribution(other, EffectRuleId{ 20 }, 0, 2, 4, 15)).target;

    const auto group = makeBattleStatusGroupPresentation(
        target.effects,
        BattleStatusKind::TrueQi);
    REQUIRE(group.quantity == 9);
    REQUIRE(group.families.size() == 2);
    CHECK(group.families[0].quantity == 7);
    CHECK(group.families[0].capacity == 10);
    CHECK(group.families[1].quantity == 2);
    CHECK(group.families[1].capacity == 4);

    REQUIRE(group.families[0].generations.size() == 1);
    REQUIRE(group.families[1].generations.size() == 1);
    CHECK(statusBehaviorsEquivalent(
        group.families[0].generations.front().behavior,
        trueQiStatusBehavior(9)));
    CHECK(statusBehaviorsEquivalent(
        group.families[1].generations.front().behavior,
        trueQiStatusBehavior(15)));
}

TEST_CASE("BattleStatusSystem presents one family cap once across immutable generations",
          "[battle][status][contribution][presentation][generation]")
{
    const EffectSourceBinding firstAlias{
        .kind = EffectSourceKind::Magic,
        .sourceId = 106,
        .ownerUnitId = 1,
        .sourceTeam = 0,
        .runtimeInstanceId = 1,
    };
    auto secondAlias = firstAlias;
    secondAlias.runtimeInstanceId = 2;
    BattleStatusSystem system({});
    auto target = system.apply(
        statusTarget(5),
        layeredContribution(firstAlias, EffectRuleId{ 10 }, 0, 6, 10, 9)).target;
    target = system.apply(
        std::move(target),
        layeredContribution(secondAlias, EffectRuleId{ 10 }, 0, 8, 10, 12)).target;

    const auto group = makeBattleStatusGroupPresentation(
        target.effects, BattleStatusKind::TrueQi);
    REQUIRE(group.quantity == 10);
    REQUIRE(group.families.size() == 1);
    CHECK(group.families.front().quantity == 10);
    CHECK(group.families.front().capacity == 10);
    REQUIRE(group.families.front().generations.size() == 2);
    CHECK(group.families.front().generations[0].quantity == 6);
    CHECK(group.families.front().generations[1].quantity == 4);
    CHECK(statusBehaviorsEquivalent(
        group.families.front().generations[0].behavior,
        trueQiStatusBehavior(9)));
    CHECK(statusBehaviorsEquivalent(
        group.families.front().generations[1].behavior,
        trueQiStatusBehavior(12)));
}

TEST_CASE("BattleStatusSystem bounds value generations by one holder-local producer family",
          "[battle][status][contribution][capacity]")
{
    const EffectSourceBinding firstAlias{
        .kind = EffectSourceKind::Magic,
        .sourceId = 106,
        .ownerUnitId = 1,
        .sourceTeam = 0,
        .runtimeInstanceId = 1,
    };
    auto secondAlias = firstAlias;
    secondAlias.runtimeInstanceId = 2;
    BattleStatusSystem system({});
    auto firstHolder = statusTarget(5);
    firstHolder = system.apply(
        std::move(firstHolder),
        layeredContribution(firstAlias, EffectRuleId{ 10 }, 0, 6, 10, 9)).target;
    firstHolder = system.apply(
        std::move(firstHolder),
        layeredContribution(secondAlias, EffectRuleId{ 10 }, 0, 8, 10, 12)).target;

    REQUIRE(firstHolder.effects.statuses.size() == 2);
    CHECK(firstHolder.effects.statuses[0].stacks == 6);
    CHECK(firstHolder.effects.statuses[1].stacks == 4);
    const auto firstSnapshot = system.snapshot(firstHolder);
    CHECK(firstSnapshot.stacks(BattleStatusKind::TrueQi) == 10);
    CHECK(firstSnapshot.familyCapacity(BattleStatusKind::TrueQi) == 10);

    auto secondHolder = statusTarget(6);
    secondHolder = system.apply(
        std::move(secondHolder),
        layeredContribution(firstAlias, EffectRuleId{ 10 }, 0, 10, 10, 9)).target;
    REQUIRE(secondHolder.effects.statuses.size() == 1);
    CHECK(secondHolder.effects.statuses.front().stacks == 10);
    CHECK(system.snapshot(secondHolder).familyCapacity(BattleStatusKind::TrueQi) == 10);
}

TEST_CASE("BattleStatusSystem rejects a drifting limit in one producer family",
          "[battle][status][contribution][capacity][invariant]")
{
    const EffectSourceBinding source{
        .kind = EffectSourceKind::Magic,
        .sourceId = 106,
        .ownerUnitId = 1,
        .sourceTeam = 0,
    };
    BattleStatusSystem system({});
    auto target = system.apply(
        statusTarget(5),
        layeredContribution(source, EffectRuleId{ 10 }, 0, 6, 10, 9)).target;

    auto inconsistent = layeredContribution(
        source,
        EffectRuleId{ 10 },
        0,
        1,
        12,
        9);
    inconsistent.stack = EffectStackPolicy::Replace;
    CHECK_THROWS_AS(
        system.apply(std::move(target), inconsistent),
        std::invalid_argument);
}

TEST_CASE("BattleStatusSystem always replaces the holder-local selected debuff packet",
          "[battle][status][contribution][replace-group]")
{
    const EffectSourceBinding first{
        .kind = EffectSourceKind::Magic,
        .sourceId = 39,
        .ownerUnitId = 1,
        .sourceTeam = 0,
    };
    const EffectSourceBinding second{
        .kind = EffectSourceKind::Equipment,
        .sourceId = 901,
        .ownerUnitId = 1,
        .sourceTeam = 0,
    };
    const auto sevenStarRequest = [](EffectSourceBinding source, EffectRuleId ruleId)
    {
        return catalogDebuff(
            BattleStatusKind::SevenStarMark,
            source,
            ruleId,
            7,
            150);
    };

    BattleStatusSystem system({});
    auto target = system.apply(
        statusTarget(5),
        sevenStarRequest(first, EffectRuleId{ 10 })).target;
    target = system.apply(
        std::move(target),
        sevenStarRequest(second, EffectRuleId{ 20 })).target;
    REQUIRE(target.effects.statuses.size() == 1);
    CHECK(target.effects.statuses.front().producerFamily
        == sevenStarRequest(second, EffectRuleId{ 20 }).producerFamily);

    auto replacement = sevenStarRequest(first, EffectRuleId{ 10 });
    const auto replaced = system.apply(std::move(target), replacement);
    CHECK(replaced.outcome == BattleStatusApplyOutcome::Replaced);
    REQUIRE(replaced.target.effects.statuses.size() == 1);
    CHECK(replaced.target.effects.statuses.front().producerFamily
        == replacement.producerFamily);
}

TEST_CASE("BattleStatusSystem bleed group ratchets its holder-local cap without resetting its clock or attribution at cap",
          "[battle][status][debuff][bleed][lifecycle]")
{
    const EffectSourceBinding quickBlade{
        .kind = EffectSourceKind::Combo,
        .sourceId = 20,
        .ownerUnitId = 1,
    };
    const EffectSourceBinding mandarinBlade{
        .kind = EffectSourceKind::Magic,
        .sourceId = 62,
        .ownerUnitId = 2,
    };
    BattleStatusSystem system({});

    auto created = system.apply(
        statusTarget(5),
        sharedBleed(quickBlade, EffectRuleId{ 1 }, 2, 3));
    CHECK(created.applied);
    REQUIRE(created.target.effects.statuses.size() == 1);
    auto* bleed = created.target.effects.find(BattleStatusKind::Bleed);
    REQUIRE(bleed);
    CHECK(bleed->stacks == 2);
    CHECK(bleed->targetTotalLimit == 3);
    REQUIRE(bleed->behaviorRuntime.size() == 1);
    CHECK(bleed->behaviorRuntime.front().intervalFramesRemaining == 10);
    const auto groupStorageSequence = bleed->appliedSequence;
    const auto creationNegativeSequence = bleed->negativeEffectSequence;
    bleed->behaviorRuntime.front().intervalFramesRemaining = 7;

    auto added = system.apply(
        std::move(created.target),
        sharedBleed(mandarinBlade, EffectRuleId{ 2 }, 1, 1));
    CHECK(added.applied);
    bleed = added.target.effects.find(BattleStatusKind::Bleed);
    REQUIRE(bleed);
    CHECK(bleed->stacks == 3);
    CHECK(bleed->targetTotalLimit == 3);
    CHECK(bleed->sourceUnitId == 2);
    CHECK(bleed->appliedSequence == groupStorageSequence);
    CHECK(bleed->negativeEffectSequence > creationNegativeSequence);
    CHECK(bleed->behaviorRuntime.front().intervalFramesRemaining == 7);
    const auto fullSequence = bleed->appliedSequence;
    const auto fullNegativeSequence = bleed->negativeEffectSequence;
    const auto fullProducer = bleed->producer;
    const auto fullOrigin = bleed->origin;

    auto rejected = system.apply(
        std::move(added.target),
        sharedBleed(quickBlade, EffectRuleId{ 3 }, 1, 2));
    CHECK_FALSE(rejected.applied);
    bleed = rejected.target.effects.find(BattleStatusKind::Bleed);
    REQUIRE(bleed);
    CHECK(bleed->stacks == 3);
    CHECK(bleed->targetTotalLimit == 3);
    CHECK(bleed->appliedSequence == fullSequence);
    CHECK(bleed->negativeEffectSequence == fullNegativeSequence);
    CHECK(bleed->producer == fullProducer);
    CHECK(bleed->origin == fullOrigin);
    CHECK(bleed->behaviorRuntime.front().intervalFramesRemaining == 7);

    auto ratcheted = system.apply(
        std::move(rejected.target),
        sharedBleed(quickBlade, EffectRuleId{ 4 }, 1, 5));
    CHECK(ratcheted.applied);
    bleed = ratcheted.target.effects.find(BattleStatusKind::Bleed);
    REQUIRE(bleed);
    CHECK(bleed->stacks == 4);
    CHECK(bleed->targetTotalLimit == 5);
    CHECK(bleed->sourceUnitId == 1);
    CHECK(bleed->appliedSequence == groupStorageSequence);
    CHECK(bleed->behaviorRuntime.front().intervalFramesRemaining == 7);
    const auto presentation = makeBattleStatusGroupPresentation(
        ratcheted.target.effects,
        BattleStatusKind::Bleed);
    CHECK(presentation.quantity == 4);
    CHECK(presentation.targetTotalCapacity == 5);

    auto removed = system.remove(
        std::move(ratcheted.target),
        { .statuses = { BattleStatusKind::Bleed } });
    CHECK(removed.removedCount == 1);
    CHECK_FALSE(removed.target.effects.has(BattleStatusKind::Bleed));

    auto fresh = system.apply(
        std::move(removed.target),
        sharedBleed(mandarinBlade, EffectRuleId{ 5 }, 1, 1));
    bleed = fresh.target.effects.find(BattleStatusKind::Bleed);
    REQUIRE(bleed);
    CHECK(bleed->stacks == 1);
    CHECK(bleed->targetTotalLimit == 1);
    CHECK(bleed->sourceUnitId == 2);
    CHECK(bleed->behaviorRuntime.front().intervalFramesRemaining == 10);
    CHECK(bleed->remainingFrames == 0);
}

TEST_CASE("BattleStatusSystem selected fixed debuffs replace one complete packet and keep catalog behavior",
          "[battle][status][debuff][selected][catalog]")
{
    const EffectSourceBinding first{
        .kind = EffectSourceKind::Magic,
        .sourceId = 11,
        .ownerUnitId = 1,
        .sourceTeam = 0,
    };
    const EffectSourceBinding second{
        .kind = EffectSourceKind::Equipment,
        .sourceId = 901,
        .ownerUnitId = 2,
        .sourceTeam = 0,
    };
    BattleStatusSystem system({});

    for (const auto kind : {
             BattleStatusKind::ColdPoison,
             BattleStatusKind::WitheredBone,
         })
    {
        DYNAMIC_SECTION(battleStatusLabel(kind))
        {
            auto target = system.apply(
                statusTarget(5),
                catalogDebuff(kind, first, EffectRuleId{ 1 }, 1, 30)).target;
            auto refreshed = system.apply(
                std::move(target),
                catalogDebuff(kind, second, EffectRuleId{ 2 }, 1, 90));
            CHECK(refreshed.outcome == BattleStatusApplyOutcome::Refreshed);
            REQUIRE(refreshed.target.effects.statuses.size() == 1);
            const auto& selected = refreshed.target.effects.statuses.front();
            CHECK(selected.remainingFrames == 90);
            CHECK(selected.sourceUnitId == 2);
            CHECK(selected.producer->binding == second);

            const auto snapshot = system.snapshot(refreshed.target);
            if (kind == BattleStatusKind::ColdPoison)
            {
                CHECK(snapshot.speedPctDelta == -25);
                CHECK(battleStatusHealModifiers(
                    snapshot,
                    BattleHealKind::Direct).blocked);
            }
            else
            {
                CHECK(snapshot.damageTakenPct == 25);
                CHECK(battleStatusHealModifiers(
                    snapshot,
                    BattleHealKind::Direct).receivedHealPcts
                    == std::vector<int>{ 25 });
            }
        }
    }
}

TEST_CASE("BattleStatusSystem selected duration refresh uses the accepted post-protection duration exactly",
          "[battle][status][debuff][selected][duration][protection]")
{
    const EffectSourceBinding first{
        .kind = EffectSourceKind::Magic,
        .sourceId = 11,
        .ownerUnitId = 1,
    };
    const EffectSourceBinding second{
        .kind = EffectSourceKind::Equipment,
        .sourceId = 901,
        .ownerUnitId = 2,
    };
    BattleStatusSystem system({});
    for (const auto kind : {
             BattleStatusKind::ColdPoison,
             BattleStatusKind::WitheredBone,
         })
    {
        DYNAMIC_SECTION(battleStatusLabel(kind))
        {
            auto target = system.apply(
                statusTarget(5),
                catalogDebuff(kind, first, EffectRuleId{ 1 }, 1, 80)).target;
            target.effects.statusShield = 10;

            auto protectedRefresh = system.apply(
                std::move(target),
                catalogDebuff(kind, second, EffectRuleId{ 2 }, 1, 80));
            CHECK(protectedRefresh.applied);
            CHECK(protectedRefresh.appliedDurationFrames == 70);
            REQUIRE(protectedRefresh.target.effects.statuses.size() == 1);
            CHECK(protectedRefresh.target.effects.statuses.front().remainingFrames == 70);
            CHECK(protectedRefresh.target.effects.statuses.front().sourceUnitId == 2);

            protectedRefresh.target.effects.statusShield = 90;
            const auto beforeBlocked = protectedRefresh.target.effects.statuses.front();
            const auto blocked = system.apply(
                std::move(protectedRefresh.target),
                catalogDebuff(kind, first, EffectRuleId{ 3 }, 1, 90));
            CHECK_FALSE(blocked.applied);
            CHECK(blocked.outcome == BattleStatusApplyOutcome::BlockedByStatusShield);
            REQUIRE(blocked.target.effects.statuses.size() == 1);
            CHECK(blocked.target.effects.statuses.front() == beforeBlocked);
        }
    }
}

TEST_CASE("BattleStatusSystem seven-star replacement is atomic allows downgrade and respects blocked applications",
          "[battle][status][debuff][selected][seven_star][protection]")
{
    const EffectSourceBinding first{
        .kind = EffectSourceKind::Magic,
        .sourceId = 39,
        .ownerUnitId = 1,
    };
    const EffectSourceBinding second{
        .kind = EffectSourceKind::Equipment,
        .sourceId = 903,
        .ownerUnitId = 2,
    };
    BattleStatusSystem system({});
    auto target = system.apply(
        statusTarget(5),
        catalogDebuff(
            BattleStatusKind::SevenStarMark,
            first,
            EffectRuleId{ 1 },
            7,
            150)).target;
    const auto original = target.effects.statuses.front();
    target.effects.statusShield = 100;

    const auto blocked = system.apply(
        std::move(target),
        catalogDebuff(
            BattleStatusKind::SevenStarMark,
            second,
            EffectRuleId{ 2 },
            3,
            100));
    CHECK_FALSE(blocked.applied);
    REQUIRE(blocked.target.effects.statuses.size() == 1);
    CHECK(blocked.target.effects.statuses.front() == original);

    auto unprotected = blocked.target;
    unprotected.effects.statusShield = 0;
    const auto downgraded = system.apply(
        std::move(unprotected),
        catalogDebuff(
            BattleStatusKind::SevenStarMark,
            second,
            EffectRuleId{ 2 },
            3,
            100));
    CHECK(downgraded.outcome == BattleStatusApplyOutcome::Replaced);
    REQUIRE(downgraded.target.effects.statuses.size() == 1);
    const auto& replacement = downgraded.target.effects.statuses.front();
    CHECK(replacement.stacks == 3);
    CHECK(replacement.remainingFrames == 100);
    CHECK(replacement.sourceUnitId == 2);
    CHECK(replacement.producer->binding == second);
    CHECK(replacement.appliedSequence != original.appliedSequence);
    CHECK(replacement.negativeEffectSequence != original.negativeEffectSequence);
}

TEST_CASE("BattleStatusSystem neutralize-force and blinded reapplications replace their selected charge packets",
          "[battle][status][debuff][selected][suppression]")
{
    const EffectSourceBinding first{
        .kind = EffectSourceKind::Magic,
        .sourceId = 80,
        .ownerUnitId = 1,
    };
    const EffectSourceBinding second{
        .kind = EffectSourceKind::Equipment,
        .sourceId = 902,
        .ownerUnitId = 2,
    };
    BattleStatusSystem system({});
    for (const auto kind : {
             BattleStatusKind::NeutralizeForce,
             BattleStatusKind::Blinded,
         })
    {
        DYNAMIC_SECTION(battleStatusLabel(kind))
        {
            auto target = system.apply(
                statusTarget(5),
                catalogDebuff(
                    kind,
                    first,
                    EffectRuleId{ 1 },
                    3,
                    0,
                    kind == BattleStatusKind::NeutralizeForce
                        ? std::optional{ 10 }
                        : std::nullopt)).target;
            auto replacementRequest = catalogDebuff(
                kind,
                second,
                EffectRuleId{ 2 },
                1,
                0,
                kind == BattleStatusKind::NeutralizeForce
                    ? std::optional{ 20 }
                    : std::nullopt);
            const auto replaced = system.apply(
                std::move(target),
                replacementRequest);
            CHECK(replaced.outcome == BattleStatusApplyOutcome::Replaced);
            REQUIRE(replaced.target.effects.statuses.size() == 1);
            const auto& selected = replaced.target.effects.statuses.front();
            CHECK(selected.stacks == 1);
            CHECK(selected.sourceUnitId == 2);
            CHECK(selected.producer->binding == second);
            CHECK(statusBehaviorsEquivalent(
                selected.behavior,
                replacementRequest.behavior));
        }
    }
}

TEST_CASE("BattleStatusSystem scopes one producer family allocation to each holder",
          "[battle][status][contribution][capacity][holder]")
{
    const EffectSourceBinding source{
        .kind = EffectSourceKind::Equipment,
        .sourceId = 801,
        .ownerUnitId = 1,
        .sourceTeam = 0,
    };
    const StatusProducerKey producer{ source, EffectRuleId{ 44 }, 2 };
    const StatusProducerFamilyKey family{
        source.kind,
        source.sourceId,
        source.ownerUnitId,
        EffectRuleId{ 44 },
        2,
    };
    const auto applyCharge = [&](BattleStatusUnitState holder)
    {
        BattleStatusApplyRequest request;
        request.kind = BattleStatusKind::NextAttackMiss;
        request.producer = producer;
        request.producerFamily = family;
        request.behavior = attackSuppressionStatusBehavior(
            BattleStatusKind::NextAttackMiss);
        request.sourceUnitId = source.ownerUnitId;
        request.stacks = 1;
        request.origin = BattleStatusEffectOrigin{
            source, EffectRuleId{ 44 }, 9 };
        request.stack = EffectStackPolicy::Replace;
        request.stackLimit = 1;
        return BattleStatusSystem({}).apply(std::move(holder), request).target;
    };

    auto first = applyCharge(statusTarget(10));
    auto second = applyCharge(statusTarget(11));
    auto third = applyCharge(statusTarget(12));
    REQUIRE(first.effects.statuses.size() == 1);
    REQUIRE(second.effects.statuses.size() == 1);
    REQUIRE(third.effects.statuses.size() == 1);
    CHECK(first.effects.statuses.front().stacks == 1);
    CHECK(second.effects.statuses.front().stacks == 1);
    CHECK(third.effects.statuses.front().stacks == 1);

    const auto consumed = BattleStatusSystem({}).consume(
        std::move(second),
        { .kind = BattleStatusKind::NextAttackMiss });
    CHECK(consumed.consumed);
    CHECK_FALSE(consumed.target.effects.has(BattleStatusKind::NextAttackMiss));
    CHECK(first.effects.has(BattleStatusKind::NextAttackMiss));
    CHECK(third.effects.has(BattleStatusKind::NextAttackMiss));
}

TEST_CASE("BattleStatusSystem group duration queries reduce every contribution",
          "[battle][status][contribution][query]")
{
    BattleStatusEffectState effects;
    effects.statuses = {
        {
            .kind = BattleStatusKind::TrueQi,
            .remainingFrames = 30,
            .maximumFrames = 60,
            .stacks = 1,
            .appliedSequence = 1,
        },
        {
            .kind = BattleStatusKind::TrueQi,
            .remainingFrames = 90,
            .maximumFrames = 120,
            .stacks = 1,
            .appliedSequence = 2,
        },
    };
    CHECK(effects.remainingFrames(BattleStatusKind::TrueQi) == 90);
    CHECK(effects.maximumFrames(BattleStatusKind::TrueQi) == 120);
}

TEST_CASE("BattleStatusSystem rounds a bound persistent expression after layer scaling",
          "[battle][status][contribution][number][rounding]")
{
    ModifyDamageAction outgoing;
    outgoing.perspective = DamageModifierPerspective::Outgoing;
    outgoing.stage = DamageModifierStage::BeforeDefense;
    outgoing.channel = DamageChannel::Skill;
    outgoing.operation = DamageModifierOperation::PercentAdd;
    outgoing.amount = {
        .base = EffectNumberBase::BoundRatio,
        .percent = 3,
        .statusScale = StatusNumberScale::PerContributionLayer,
        .boundNumerator = 50,
        .boundDenominator = 1,
    };

    BattleStatusEffectState effects;
    effects.statuses.push_back(persistentContribution(
        BattleStatusKind::BattleSpirit,
        persistentStatusBehavior({ EffectAction{ outgoing } }),
        EffectSourceKind::Magic,
        8101,
        0,
        2,
        effects.nextStatusSequence++));

    // floor((50 * 3% + 0) * 2) == 3. Rounding each layer first
    // would incorrectly produce 2.
    CHECK(BattleStatusSystem({}).snapshot(effects).skillDamagePct == 3);
}

TEST_CASE("BattleStatusSystem keeps persistent heal modifiers scoped to their heal kinds",
          "[battle][status][heal][kind]")
{
    ModifyHealTransactionAction blockDirect;
    blockDirect.operation = HealModifierOperation::Block;
    blockDirect.kinds = { "直接" };
    ModifyHealTransactionAction reduceTeam;
    reduceTeam.operation = HealModifierOperation::MultiplyReceived;
    reduceTeam.percent = 25;
    reduceTeam.kinds = { "隊伍" };

    BattleStatusEffectState effects;
    effects.statuses.push_back(persistentContribution(
        BattleStatusKind::Shadowless,
        persistentStatusBehavior({
            EffectAction{ blockDirect },
            EffectAction{ reduceTeam },
        }),
        EffectSourceKind::Magic,
        8102,
        0,
        1,
        effects.nextStatusSequence++));
    const auto status = BattleStatusSystem({}).snapshot(effects);

    const auto direct = battleStatusHealModifiers(status, BattleHealKind::Direct);
    CHECK(direct.blocked);
    CHECK(direct.receivedHealPcts.empty());

    const auto team = battleStatusHealModifiers(status, BattleHealKind::Team);
    CHECK_FALSE(team.blocked);
    CHECK(team.receivedHealPcts == std::vector<int>{ 25 });
}

TEST_CASE("Persistent status healing and speed effects preserve ordering gates and rounding",
          "[battle][status][persistent][heal][speed][parity]")
{
    BattleStatusEffectState withered;
    withered.statuses.push_back(persistentContribution(
        BattleStatusKind::Shadowless,
        witheredBoneStatusBehavior(0, 33),
        EffectSourceKind::Magic,
        8103,
        0,
        1,
        withered.nextStatusSequence++));
    withered.statuses.push_back(persistentContribution(
        BattleStatusKind::Shadowless,
        witheredBoneStatusBehavior(0, 50),
        EffectSourceKind::Magic,
        8103,
        1,
        1,
        withered.nextStatusSequence++));
    const auto witheredSnapshot = BattleStatusSystem({}).snapshot(withered);
    const auto ordered = battleStatusHealModifiers(
        witheredSnapshot,
        BattleHealKind::Direct);
    CHECK(ordered.receivedHealPcts == std::vector<int>{ 67, 50 });

    BattleHealRequest request;
    request.sourceUnitId = 1;
    request.targetUnitId = 2;
    request.kind = BattleHealKind::Direct;
    request.amount = fixedHealAmount(5);
    const auto rounded = resolveHeal(
        request,
        { .id = 1, .alive = true, .hp = 100, .maxHp = 100 },
        { .id = 2, .alive = true, .hp = 1, .maxHp = 100 },
        ordered);
    CHECK(rounded.calculatedAmount == 5);
    CHECK(rounded.modifiedAmount == 1);
    CHECK(rounded.appliedAmount == 1);

    ModifyAttributeAction speed;
    speed.attribute = BattleAttribute::Speed;
    speed.operation = AttributeOperation::PercentAdd;
    speed.amount.flat = -25;
    ModifyHealTransactionAction block;
    block.operation = HealModifierOperation::Block;
    block.kinds = { "直接" };
    BattleStatusEffectState coldPoison;
    coldPoison.statuses.push_back(persistentContribution(
        BattleStatusKind::Shadowless,
        persistentStatusBehavior({
            EffectAction{ speed },
            EffectAction{ block },
        }),
        EffectSourceKind::Magic,
        8104,
        0,
        1,
        coldPoison.nextStatusSequence++));
    const auto coldSnapshot = BattleStatusSystem({}).snapshot(coldPoison);
    CHECK(coldSnapshot.speedPctDelta == -25);
    const auto blockedModifiers = battleStatusHealModifiers(
        coldSnapshot,
        BattleHealKind::Direct);
    REQUIRE(blockedModifiers.blocked);
    const auto blocked = resolveHeal(
        request,
        { .id = 1, .alive = true, .hp = 100, .maxHp = 100 },
        { .id = 2, .alive = true, .hp = 1, .maxHp = 100 },
        blockedModifiers);
    CHECK(blocked.outcome == BattleHealOutcome::Blocked);
    CHECK(blocked.modifiedAmount == 0);
    CHECK(blocked.appliedAmount == 0);
}

TEST_CASE("Persistent status heal modifiers use structured cross-source order",
          "[battle][status][persistent][heal][ordering]")
{
    ModifyHealTransactionAction comboHalf;
    comboHalf.operation = HealModifierOperation::MultiplyReceived;
    comboHalf.percent = 50;
    comboHalf.kinds = { "直接" };
    ModifyHealTransactionAction equipmentTwoThirds;
    equipmentTwoThirds.operation = HealModifierOperation::MultiplyReceived;
    equipmentTwoThirds.percent = 67;
    equipmentTwoThirds.kinds = { "直接" };

    BattleStatusEffectState effects;
    // Apply equipment first to prove appliedSequence does not override the
    // normative Combo -> Equipment source precedence.
    effects.statuses.push_back(persistentContribution(
        BattleStatusKind::Shadowless,
        persistentStatusBehavior({ EffectAction{ equipmentTwoThirds } }),
        EffectSourceKind::Equipment,
        8201,
        0,
        1,
        effects.nextStatusSequence++));
    effects.statuses.push_back(persistentContribution(
        BattleStatusKind::Shadowless,
        persistentStatusBehavior({ EffectAction{ comboHalf } }),
        EffectSourceKind::Combo,
        8202,
        0,
        1,
        effects.nextStatusSequence++));

    const auto snapshot = BattleStatusSystem({}).snapshot(effects);
    const auto modifiers = battleStatusHealModifiers(snapshot, BattleHealKind::Direct);
    CHECK(modifiers.receivedHealPcts == std::vector<int>{ 50, 67 });

    BattleHealRequest request;
    request.sourceUnitId = 1;
    request.targetUnitId = 2;
    request.kind = BattleHealKind::Direct;
    request.amount = fixedHealAmount(3);
    const auto healed = resolveHeal(
        request,
        { .id = 1, .alive = true, .hp = 100, .maxHp = 100 },
        { .id = 2, .alive = true, .hp = 1, .maxHp = 100 },
        modifiers);
    CHECK(healed.modifiedAmount == 0);
    CHECK(healed.outcome == BattleHealOutcome::ZeroAfterModifier);
}

TEST_CASE("Persistent status aggregates saturate across independent producers",
          "[battle][status][persistent][boundary]")
{
    ModifyAttributeAction speed;
    speed.attribute = BattleAttribute::Speed;
    speed.operation = AttributeOperation::PercentAdd;
    speed.amount.flat = std::numeric_limits<int>::max();

    ModifyDamageAction outgoing;
    outgoing.perspective = DamageModifierPerspective::Outgoing;
    outgoing.stage = DamageModifierStage::BeforeDefense;
    outgoing.channel = DamageChannel::Skill;
    outgoing.operation = DamageModifierOperation::PercentAdd;
    outgoing.amount.flat = std::numeric_limits<int>::max();

    ModifyDamageAction reduction;
    reduction.perspective = DamageModifierPerspective::Incoming;
    reduction.stage = DamageModifierStage::BeforeDefense;
    reduction.channel = DamageChannel::All;
    reduction.operation = DamageModifierOperation::PercentAdd;
    reduction.amount.flat = std::numeric_limits<int>::min();

    ModifyDamageAction taken;
    taken.perspective = DamageModifierPerspective::Incoming;
    taken.stage = DamageModifierStage::Final;
    taken.channel = DamageChannel::All;
    taken.operation = DamageModifierOperation::PercentAdd;
    taken.amount.flat = std::numeric_limits<int>::max();

    const auto behavior = persistentStatusBehavior({
        EffectAction{ speed },
        EffectAction{ outgoing },
        EffectAction{ reduction },
        EffectAction{ taken },
    });
    BattleStatusEffectState effects;
    for (int sourceId : { 8301, 8302 })
    {
        effects.statuses.push_back(persistentContribution(
            BattleStatusKind::BattleSpirit,
            behavior,
            EffectSourceKind::Magic,
            sourceId,
            0,
            1,
            effects.nextStatusSequence++));
    }

    const auto snapshot = BattleStatusSystem({}).snapshot(effects);
    CHECK(snapshot.speedPctDelta == std::numeric_limits<int>::max());
    CHECK(snapshot.skillDamagePct == std::numeric_limits<int>::max());
    CHECK(snapshot.damageReductionPct == std::numeric_limits<int>::max());
    CHECK(snapshot.damageTakenPct == std::numeric_limits<int>::max());
}

TEST_CASE("BattleStatusSystem_CopiesStatusEffectsAsACluster", "[battle][status]")
{
    BattleStatusUnitState source;
    source.id = 7;
    source.effects.statuses = {
        boundStatusBehaviorContribution(
            BattleStatusKind::Poison,
            poisonStatusBehavior(5),
            3,
            9301,
            2,
            1,
            9),
        { .kind = BattleStatusKind::Bleed, .stacks = 2 },
        { .kind = BattleStatusKind::Stun, .remainingFrames = 4 },
        { .kind = BattleStatusKind::MpBlocked, .remainingFrames = 6 },
    };

    auto runtime = makeBattleStatusRuntimeUnit(source);

    CHECK(runtime.effects == source.effects);
}

TEST_CASE("BattleStatusSystem projects extreme poison damage without overflow",
          "[battle][status][poison][projection][boundary]")
{
    CHECK(projectRemainingPoisonDamage({
        .framesUntilNextTick = 30,
        .remainingFrames = 30,
        .remainingStacks = 1,
        .intervalFrames = 30,
        .currentHp = std::numeric_limits<int>::max(),
        .damagePct = std::numeric_limits<int>::max(),
    }) == std::numeric_limits<int>::max());
}

TEST_CASE("BattleFrameRunner_NextAttackMissIsConsumedFromTheDefender", "[battle][core][status][suppression]")
{
    auto state = attackSuppressionFrameState();
    addAttackSuppressionStatus(state, 0, BattleStatusKind::NextAttackMiss);
    addAttackSuppressionStatus(state, 1, BattleStatusKind::NextAttackMiss);
    const auto spawned = spawnTrackedAttack(state, attackSuppressionRequest());

    advanceUntilAttackContacts(state, spawned.attackId, 2);

    CHECK(state.units.requireCore(1).vitals.hp == 100);
    CHECK(state.units.requireCore(2).vitals.hp == 75);
    CHECK(hasAttackSuppressionStatus(state, 0, BattleStatusKind::NextAttackMiss));
    CHECK_FALSE(hasAttackSuppressionStatus(state, 1, BattleStatusKind::NextAttackMiss));
    CHECK_FALSE(state.attacks.contactsSuppressed(spawned.attackId));
    CHECK(requireById(state.attacks.attacks, spawned.attackId).hitUnitIds
          == std::vector<int>{ 1, 2 });
}

TEST_CASE("BattleFrameRunner_BlindedSuppressesEveryContactOfOneAttack", "[battle][core][status][suppression]")
{
    auto state = attackSuppressionFrameState();
    addAttackContactShieldRule(state, 9);
    addAttackSuppressionStatus(state, 0, BattleStatusKind::Blinded);
    const auto blindedAttack = spawnTrackedAttack(state, attackSuppressionRequest());

    advanceUntilAttackContacts(state, blindedAttack.attackId, 2);

    CHECK(state.units.requireCore(1).vitals.hp == 100);
    CHECK(state.units.requireCore(2).vitals.hp == 100);
    CHECK(state.units.requireCore(1).shield == 0);
    CHECK(state.units.requireCore(2).shield == 0);
    CHECK(state.effectRules.activationCount(
        {
            .kind = EffectSourceKind::Combo,
            .sourceId = 777,
            .ownerUnitId = 0,
            .sourceTeam = 0,
        },
        EffectRuleId{ 1 }) == 0);
    CHECK_FALSE(hasAttackSuppressionStatus(state, 0, BattleStatusKind::Blinded));
    CHECK(state.attacks.contactsSuppressed(blindedAttack.attackId));

    auto nextRequest = attackSuppressionRequest();
    nextRequest.initial.preferredTargetUnitId = 1;
    nextRequest.initial.through = false;
    nextRequest.initial.position = state.units.requireCore(1).motion.position;
    nextRequest.initial.velocity = {};
    const auto nextAttack = spawnTrackedAttack(state, std::move(nextRequest));
    runBattleFrame(state);

    CHECK(state.units.requireCore(1).vitals.hp == 84);
    CHECK(state.units.requireCore(1).shield == 0);
    CHECK_FALSE(state.attacks.contactsSuppressed(nextAttack.attackId));
}

TEST_CASE("BattleFrameRunner_NeutralizeForceRestoresMpToTheActualHitTargetOnce", "[battle][core][status]")
{
    auto state = attackSuppressionFrameState();
    state.units.requireCore(1).vitals.mp = 0;
    state.units.requireCore(1).vitals.maxMp = 100;
    state.units.requireCore(2).vitals.mp = 0;
    state.units.requireCore(2).vitals.maxMp = 100;
    auto baseline = state;
    const auto baselineAttack = spawnTrackedAttack(baseline, attackSuppressionRequest(), 2);
    addAttackSuppressionStatus(state, 0, BattleStatusKind::NeutralizeForce, 50);
    const auto tracked = spawnTrackedAttack(state, attackSuppressionRequest(), 2);

    advanceUntilAttackContacts(state, tracked.attackId, 1);
    advanceUntilAttackContacts(baseline, baselineAttack.attackId, 1);

    CHECK(state.units.requireCore(1).vitals.mp == baseline.units.requireCore(1).vitals.mp + 50);
    CHECK(state.units.requireCore(2).vitals.mp == baseline.units.requireCore(2).vitals.mp);
    CHECK(state.units.requireCore(1).vitals.hp == 75);
    CHECK(state.units.requireCore(1).shield == 0);
    CHECK(state.units.requireCore(2).shield == 0);
    CHECK_FALSE(hasAttackSuppressionStatus(state, 0, BattleStatusKind::NeutralizeForce));
    CHECK_FALSE(state.attacks.contactsSuppressed(tracked.attackId));

    advanceUntilAttackContacts(state, tracked.attackId, 2);
    advanceUntilAttackContacts(baseline, baselineAttack.attackId, 2);

    CHECK(state.units.requireCore(2).vitals.hp == 75);
    CHECK(state.units.requireCore(2).vitals.mp == baseline.units.requireCore(2).vitals.mp);
    CHECK(state.castLifecycle.runtime(tracked.castId).aggregate.distinctHitUnitIds.size() == 2);
}

TEST_CASE("BattleFrameRunner_SourceSuppressionCoversDelayedSiblingAttacksUntilCastSettles", "[battle][core][status][suppression][cast]")
{
    auto state = attackSuppressionFrameState();
    addAttackContactShieldRule(state, 9);
    addAttackSuppressionStatus(state, 0, BattleStatusKind::Blinded);

    auto firstRequest = attackSuppressionRequest();
    firstRequest.initial.preferredTargetUnitId = 1;
    firstRequest.initial.requirePreferredTarget = true;
    firstRequest.initial.through = false;
    firstRequest.initial.totalFrame = 2;
    auto siblingRequest = attackSuppressionRequest();
    siblingRequest.initial.preferredTargetUnitId = 2;
    siblingRequest.initial.requirePreferredTarget = true;
    siblingRequest.initial.through = false;
    siblingRequest.initial.totalFrame = 2;
    siblingRequest.initial.position = { 120, 100, 0 };

    std::vector<BattleAttackSpawnRequest> requests;
    requests.push_back(std::move(firstRequest));
    requests.push_back(std::move(siblingRequest));
    auto tracked = reserveTrackedAttackCast(state, std::move(requests), 2);
    const auto firstSpawned = state.attacks.spawn(
        std::move(tracked.requests[0]),
        state.castLifecycle);

    runBattleFrame(state);

    CHECK(state.units.requireCore(1).vitals.hp == 100);
    CHECK(state.units.requireCore(1).shield == 0);
    CHECK(state.units.requireCore(2).shield == 0);
    CHECK_FALSE(hasAttackSuppressionStatus(
        state,
        0,
        BattleStatusKind::Blinded));
    CHECK(state.attacks.castContactsSuppressed(tracked.castId));
    CHECK(state.attacks.contactsSuppressed(firstSpawned.attackId));

    const auto siblingSpawned = state.attacks.spawn(
        std::move(tracked.requests[1]),
        state.castLifecycle);
    CHECK(state.attacks.contactsSuppressed(siblingSpawned.attackId));

    runBattleFrame(state);

    CHECK(state.units.requireCore(2).vitals.hp == 100);
    CHECK(state.units.requireCore(2).shield == 0);
    CHECK(state.castLifecycle.runtime(tracked.castId)
          .aggregate.distinctHitUnitIds.empty());

    runBattleFrame(state);

    CHECK_FALSE(state.attacks.castContactsSuppressed(tracked.castId));

    auto nextRequest = attackSuppressionRequest();
    nextRequest.initial.preferredTargetUnitId = 1;
    nextRequest.initial.requirePreferredTarget = true;
    nextRequest.initial.through = false;
    const auto nextAttack = spawnTrackedAttack(state, std::move(nextRequest));
    runBattleFrame(state);

    CHECK_FALSE(state.attacks.contactsSuppressed(nextAttack.attackId));
    CHECK(state.units.requireCore(1).vitals.hp == 84);
    CHECK(state.units.requireCore(1).shield == 0);
}

TEST_CASE("BattleFrameRunner_NoContactCancelledCastLeavesSourceSuppressionForNextHit", "[battle][core][status][suppression][cast]")
{
    auto state = attackSuppressionFrameState();
    addAttackSuppressionStatus(state, 0, BattleStatusKind::Blinded);
    const auto cancelled = state.castLifecycle.beginRootCast({
        .sourceUnitId = 0,
        .magicId = 101,
    });

    state.castLifecycle.cancelPlannedCast(cancelled, state.movement.frame);

    CHECK(hasAttackSuppressionStatus(state, 0, BattleStatusKind::Blinded));
    CHECK_FALSE(state.attacks.castContactsSuppressed(
        cancelled.provenance.castId));

    auto request = attackSuppressionRequest();
    request.initial.preferredTargetUnitId = 1;
    request.initial.requirePreferredTarget = true;
    request.initial.through = false;
    const auto tracked = spawnTrackedAttack(state, std::move(request));
    runBattleFrame(state);

    CHECK(state.units.requireCore(1).vitals.hp == 100);
    CHECK_FALSE(hasAttackSuppressionStatus(state, 0, BattleStatusKind::Blinded));
    CHECK(state.attacks.castContactsSuppressed(tracked.castId));
    CHECK(state.attacks.contactsSuppressed(tracked.attackId));
}

TEST_CASE("BattleFrameRunner_NeutralizeForceMpRecoveryCapsAtMaxMp", "[battle][core][status]")
{
    auto state = attackSuppressionFrameState();
    state.units.requireCore(1).vitals.mp = 90;
    state.units.requireCore(1).vitals.maxMp = 100;
    addAttackSuppressionStatus(state, 0, BattleStatusKind::NeutralizeForce, 70);
    const auto tracked = spawnTrackedAttack(state, attackSuppressionRequest(), 2);
    advanceUntilAttackContacts(state, tracked.attackId, 1);
    CHECK(state.units.requireCore(1).vitals.mp == 100);
    CHECK_FALSE(hasAttackSuppressionStatus(state, 0, BattleStatusKind::NeutralizeForce));
    CHECK_FALSE(state.attacks.contactsSuppressed(tracked.attackId));
}

TEST_CASE("BattleFrameRunner_BlindedPreservesNeutralizeForceAndIncomingMiss", "[battle][core][status][suppression]")
{
    const auto verify = [](bool neutralizeFirst)
    {
        auto state = attackSuppressionFrameState();
        if (neutralizeFirst)
        {
            addAttackSuppressionStatus(state, 0, BattleStatusKind::NeutralizeForce, 37);
            addAttackSuppressionStatus(state, 0, BattleStatusKind::Blinded);
        }
        else
        {
            addAttackSuppressionStatus(state, 0, BattleStatusKind::Blinded);
            addAttackSuppressionStatus(state, 0, BattleStatusKind::NeutralizeForce, 37);
        }
        addAttackSuppressionStatus(state, 1, BattleStatusKind::NextAttackMiss);
        const auto tracked = spawnTrackedAttack(
            state,
            attackSuppressionRequest(),
            2);

        int protectionCueCount{};
        constexpr int MaximumFrames = 12;
        for (int frameIndex = 0;
             frameIndex < MaximumFrames
             && requireById(state.attacks.attacks, tracked.attackId).hitUnitIds.size() < 2;
             ++frameIndex)
        {
            const auto frame = runBattleFrame(state);
            protectionCueCount += static_cast<int>(std::ranges::count_if(
                frame.visualEvents,
                [](const BattleVisualEvent& visual)
                {
                    return visual.type == BattleVisualEventType::RoleEffect
                        && visual.visualPath.starts_with(BattleCueVisualPathPrefix);
                }));
        }
        REQUIRE(requireById(state.attacks.attacks, tracked.attackId).hitUnitIds.size() == 2);

        CHECK(state.units.requireCore(1).vitals.hp == 100);
        CHECK(state.units.requireCore(2).vitals.hp == 100);
        CHECK(state.units.requireCore(1).shield == 0);
        CHECK(state.units.requireCore(2).shield == 0);
        CHECK(protectionCueCount == 0);
        CHECK_FALSE(hasAttackSuppressionStatus(
            state, 0, BattleStatusKind::Blinded));
        CHECK(hasAttackSuppressionStatus(
            state, 0, BattleStatusKind::NeutralizeForce));
        CHECK(hasAttackSuppressionStatus(state, 1, BattleStatusKind::NextAttackMiss));
        CHECK(state.attacks.contactsSuppressed(tracked.attackId));
    };

    SECTION("先套用化勁")
    {
        verify(true);
    }
    SECTION("先套用刺目")
    {
        verify(false);
    }
}

TEST_CASE("BattleFrameRunner consumes neutralize-force only when an attack hits after blindness and incoming miss",
          "[battle][core][status][suppression][charge]")
{
    auto state = attackSuppressionFrameState();
    state.units.requireCore(1).vitals.mp = 0;
    state.units.requireCore(1).vitals.maxMp = 1000;
    addAttackSuppressionStatus(
        state, 0, BattleStatusKind::NeutralizeForce, 37, 2);
    addAttackSuppressionStatus(
        state, 0, BattleStatusKind::Blinded, 0, 3);
    addAttackSuppressionStatus(
        state, 1, BattleStatusKind::NextAttackMiss, 0, 1);

    const auto performCast = [&](bool expectCastSuppressed)
    {
        auto request = attackSuppressionRequest();
        request.initial.preferredTargetUnitId = 1;
        request.initial.requirePreferredTarget = true;
        request.initial.through = false;
        request.initial.position = state.units.requireCore(1).motion.position;
        request.initial.velocity = {};
        const auto tracked = spawnTrackedAttack(state, std::move(request), 1);
        advanceUntilAttackContacts(state, tracked.attackId, 1);
        CHECK(state.attacks.contactsSuppressed(tracked.attackId)
            == expectCastSuppressed);
    };
    const auto stacks = [&](int unitId, BattleStatusKind kind)
    {
        return BattleStatusSystem({}).snapshot(
            state.units.require(unitId).statusDamageState()).stacks(kind);
    };

    performCast(true);
    CHECK(stacks(0, BattleStatusKind::NeutralizeForce) == 2);
    CHECK(stacks(0, BattleStatusKind::Blinded) == 2);
    CHECK(stacks(1, BattleStatusKind::NextAttackMiss) == 1);
    CHECK(state.units.requireCore(1).shield == 0);

    performCast(true);
    CHECK(stacks(0, BattleStatusKind::NeutralizeForce) == 2);
    CHECK(stacks(0, BattleStatusKind::Blinded) == 1);
    CHECK(stacks(1, BattleStatusKind::NextAttackMiss) == 1);
    CHECK(state.units.requireCore(1).shield == 0);

    performCast(true);
    CHECK(stacks(0, BattleStatusKind::Blinded) == 0);
    CHECK(stacks(1, BattleStatusKind::NextAttackMiss) == 1);
    CHECK(state.units.requireCore(1).shield == 0);

    performCast(false);
    CHECK(stacks(1, BattleStatusKind::NextAttackMiss) == 0);
    CHECK(state.units.requireCore(1).shield == 0);
    CHECK(stacks(0, BattleStatusKind::NeutralizeForce) == 2);
    CHECK(state.units.requireCore(1).vitals.mp == 2);

    performCast(false);
    CHECK(stacks(0, BattleStatusKind::NeutralizeForce) == 1);
    CHECK(state.units.requireCore(1).vitals.mp == 57);
    CHECK(state.units.requireCore(1).vitals.hp == 75);

    performCast(false);
    CHECK(stacks(0, BattleStatusKind::NeutralizeForce) == 0);
    CHECK(state.units.requireCore(1).vitals.mp == 112);
    CHECK(state.units.requireCore(1).vitals.hp == 50);
}

TEST_CASE("BattleFrameRunner_SourceSuppressionPropagatesThroughBounceDescendants", "[battle][core][status][suppression][bounce]")
{
    auto state = attackSuppressionFrameState();
    addAttackSuppressionStatus(state, 0, BattleStatusKind::Blinded);
    auto request = attackSuppressionRequest();
    request.initial.preferredTargetUnitId = 1;
    request.initial.requirePreferredTarget = true;
    request.initial.through = false;
    request.initial.bounceRemaining = 1;
    request.initial.bounceRange = 120;
    request.initial.bounceChancePct = 100;
    request.initial.bounceRollPct = 0;
    const auto source = spawnTrackedAttack(state, std::move(request));

    advanceUntilAttackContacts(state, source.attackId, 1);
    const auto bounce = std::ranges::find_if(
        state.attacks.attacks,
        [&source](const BattleAttackInstance& attack)
        {
            return attack.provenance.parentAttackId
                == battleAttackIdFromRuntimeId(source.attackId);
        });
    REQUIRE(bounce != state.attacks.attacks.end());
    const int bounceAttackId = bounce->id;
    CHECK(state.attacks.contactsSuppressed(source.attackId));
    CHECK(state.attacks.contactsSuppressed(bounceAttackId));

    advanceUntilAttackContacts(state, bounceAttackId, 1);

    CHECK(state.units.requireCore(1).vitals.hp == 100);
    CHECK(state.units.requireCore(2).vitals.hp == 100);
    CHECK_FALSE(hasAttackSuppressionStatus(state, 0, BattleStatusKind::Blinded));
    CHECK(state.attacks.contactsSuppressed(bounceAttackId));
}
