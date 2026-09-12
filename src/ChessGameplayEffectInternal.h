#pragma once

#include "ChessBattleEffectSemantics.h"
#include "ChessGameplayEffect.h"

#include <array>
#include <cassert>
#include <format>
#include <limits>

namespace KysChess::GameplayEffects
{

// 欄位只描述可調整的整數，不描述觸發、條件、動作或狀態機。
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
