#pragma once

#include "../ChessBattleEffectTypes.h"

#include <compare>
#include <cstdint>
#include <optional>
#include <set>
#include <vector>

namespace KysChess::Battle
{

struct BattleRuntimeState;
class BattleRuntimeUnits;
struct BattleStatusQuerySnapshot;

struct HealTransactionId
{
    std::uint64_t value{};

    auto operator<=>(const HealTransactionId&) const = default;
};

using BattleHealCastId = std::uint64_t;

using BattleHealKind = EffectHealKind;

enum class BattleHealBase
{
    TargetMaxHp,
    FinalHpDamage,
    FixedAmount,
};

enum class BattleHealRounding
{
    TowardZero,
};

enum class BattleHealSourcePolicy
{
    RequireAlive,
    AllowDead,
};

struct BattleHealAmount
{
    BattleHealBase base{};
    int baseValue{};
    int flat = 0;
    int percent = 0;
    BattleHealRounding rounding{};
    int minimum = 0;
};

struct BattleHealRequest
{
    HealTransactionId id{};
    int sourceUnitId{};
    int targetUnitId{};
    EffectSourceBinding source{};
    std::optional<BattleHealCastId> castId;
    BattleHealKind kind{};
    BattleHealAmount amount{};
    BattleHealSourcePolicy sourcePolicy{};
};

struct BattleHealUnitSnapshot
{
    int id = -1;
    bool alive = true;
    int hp{};
    int maxHp{};
};

struct BattleHealModifierState
{
    bool blocked = false;
    std::vector<int> receivedHealPcts;
};

BattleHealModifierState battleStatusHealModifiers(
    const BattleStatusQuerySnapshot& status,
    BattleHealKind kind);

enum class BattleHealEventType
{
    Attempted,
    Applied,
    Blocked,
};

struct BattleHealEvent
{
    BattleHealEventType type{};
    BattleHealRequest request;
    int calculatedAmount{};
    int appliedAmount{};
};

struct BattleHealRuntimeState
{
    std::uint64_t nextTransactionId = 1;
    std::set<HealTransactionId> committedTransactions;
    std::vector<BattleHealEvent> events;
};

enum class BattleHealOutcome
{
    Applied,
    Blocked,
    AlreadyFull,
    ZeroAfterModifier,
    IneligibleSource,
    IneligibleTarget,
};

struct BattleHealResult
{
    BattleHealRequest request{};
    BattleHealOutcome outcome{};
    int calculatedAmount{};
    int modifiedAmount{};
    int hpBefore{};
    int hpAfter{};
    int appliedAmount{};
};

// 傷害交易在自己的 snapshot 內已完成的治療。runtime 邊界只負責
// 登記 transaction ID 與發出 typed 事件，不可再次改寫 HP。
struct BattleResolvedHealTransaction
{
    BattleHealResult result;
    BattleHealUnitSnapshot sourceBefore;
    BattleHealUnitSnapshot targetBefore;
    BattleHealUnitSnapshot targetAfter;
};

BattleHealAmount targetMaxHpHealAmount(int flat, int percent, int minimum = 0);
BattleHealAmount fixedHealAmount(int amount);

BattleHealResult resolveHeal(
    const BattleHealRequest& request,
    const BattleHealUnitSnapshot& source,
    const BattleHealUnitSnapshot& target,
    const BattleHealModifierState& modifiers = {});

class BattleHealSystem
{
public:
    BattleHealResult commit(
        BattleRuntimeUnits& units,
        const BattleHealRequest& request,
        const BattleHealModifierState& modifiers = {}) const;

    BattleHealResult commit(
        BattleRuntimeState& state,
        const BattleHealRequest& request,
        const BattleHealModifierState& modifiers = {}) const;

    BattleHealResult recordResolved(
        BattleRuntimeState& state,
        BattleResolvedHealTransaction transaction) const;

    std::vector<BattleHealEvent> drainEvents(BattleRuntimeState& state) const;
};

}  // namespace KysChess::Battle
