#pragma once

#include "ChessBattleEffectSemantics.h"
#include "ChessGameplayEffect.h"

#include <array>
#include <cassert>
#include <format>
#include <limits>
#include <utility>

namespace KysChess::GameplayEffects
{

// 簡述統一為「觸發：效果」；機率放在冒號後，持續時間寫「，N幀」。
// 攻防用「+N攻／+N防」，其他屬性用「名稱+N」；執行細節只放完整說明。
inline std::string compactAttributeDescription(BattleAttribute attribute, int amount, bool percent)
{
    const auto unit = percent ? "%" : "";
    switch (attribute)
    {
    case BattleAttribute::Attack: return std::format("{:+}{}攻", amount, unit);
    case BattleAttribute::Defence: return std::format("{:+}{}防", amount, unit);
    case BattleAttribute::Speed: return std::format("速度{:+}{}", amount, unit);
    case BattleAttribute::BlockChance: return std::format("格擋{:+}{}", amount, unit);
    case BattleAttribute::DodgeChance: return std::format("閃避{:+}{}", amount, unit);
    case BattleAttribute::CriticalChance: return std::format("暴擊{:+}{}", amount, unit);
    case BattleAttribute::CriticalDamage: return std::format("暴傷{:+}{}", amount, unit);
    default: std::unreachable();
    }
}

// 欄位只描述數值與有限的玩法選項，不暴露執行期觸發、條件或狀態機。
template <class T> struct Parameter
{
    GameplayEffectParameter info;
    int T::* member;
};

template <class T> GameplayEffectRegistration registration()
{
    // 每個效果型別只有一份登錄資料。
    static constexpr auto parameters = []
    {
        std::array<GameplayEffectParameter, T::Parameters.size()> result{};
        for (std::size_t i = 0; i < result.size(); ++i) { result[i] = T::Parameters[i].info; }
        return result;
    }();
    return {T::Name,
            parameters,
            [](std::span<const int> values) -> GameplayEffect
            {
                assert(values.size() == T::Parameters.size());
                auto effect = std::make_shared<T>();
                for (std::size_t i = 0; i < T::Parameters.size(); ++i) { effect.get()->*T::Parameters[i].member = values[i]; }
                return effect;
            }};
}

inline ChangeResourceAction recoveryAmount(BattleResource resource, int flat, int perStar, int maxHpPercent,
    EffectNumberBase hpBase = EffectNumberBase::TargetMaxHp)
{
    ChangeResourceAction action{
        .resource = resource,
        .amount = EffectNumber{ .base = hpBase, .flat = flat, .percent = maxHpPercent },
        .kind = resource == BattleResource::Hp ? ResourceChangeKind::Restore : ResourceChangeKind::Grant
    };
    if (perStar != 0)
    {
        action.additionalAmount = EffectNumber{ .base = EffectNumberBase::SourceStar, .percent = perStar * 100 };
    }
    return action;
}

inline std::string recoveryDescription(int flat, int perStar, int maxHpPercent)
{
    std::string result;
    const auto append = [&](std::string term)
    {
        if (!result.empty())
        {
            result += "+";
        }
        result += term;
    };
    if (flat != 0)
    {
        append(std::to_string(flat));
    }
    if (perStar != 0)
    {
        append(std::format("{}×星級", perStar));
    }
    if (maxHpPercent != 0)
    {
        append(std::format("血上限{}%", maxHpPercent));
    }
    return result.empty() ? "0" : result;
}

inline ApplyStatusAction catalogStatus(ApplyStatusAction action)
{
    action.behavior = makeCatalogOwnedStatusBehavior(action);
    return action;
}

void appendAttributeEffects(std::vector<GameplayEffectRegistration>& entries);
void appendAttackEffects(std::vector<GameplayEffectRegistration>& entries);
void appendStatusEffects(std::vector<GameplayEffectRegistration>& entries);
void appendRecoveryEffects(std::vector<GameplayEffectRegistration>& entries);
void appendTacticalEffects(std::vector<GameplayEffectRegistration>& entries);

}    // namespace KysChess::GameplayEffects
