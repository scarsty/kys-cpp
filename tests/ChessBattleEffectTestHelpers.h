#pragma once

#include "ChessBattleEffectParser.h"
#include "ChessBattleEffectSemantics.h"
#include "ChessBattleEffectValidation.h"
#include "ChessEffectDescription.h"
#include "ChessGameContent.h"
#include "ChessGameSessionTestHelpers.h"

#include <catch2/catch_test_macros.hpp>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace KysChess::Test
{

inline const ChessMagicEffectDefinition& definitionWithId(
    const std::vector<ChessMagicEffectDefinition>& definitions,
    int magicId)
{
    const auto it = std::ranges::find(definitions, magicId, &ChessMagicEffectDefinition::magicId);
    REQUIRE(it != definitions.end());
    return *it;
}

inline const EffectRule& ruleWithEvent(
    const ChessMagicEffectDefinition& definition,
    EffectEvent event,
    std::size_t occurrence = 0)
{
    for (const auto& rule : definition.rules)
    {
        if (rule.event != event) continue;
        if (occurrence == 0) return rule;
        --occurrence;
    }
    INFO("武功 " << definition.magicId
        << " 找不到事件 " << static_cast<int>(event)
        << " 的第 " << occurrence << " 個規則");
    FAIL("找不到指定效果事件");
}

inline std::set<int> poolUltimateMagicIds(const ChessGameContent& content)
{
    std::set<int> result;
    for (const int roleId : content.poolRoleIds())
    {
        const auto* role = content.role(roleId);
        REQUIRE(role != nullptr);
        int roleUltimateId = -1;
        for (int star = 1; star <= 3; ++star)
        {
            const auto magics = chessRoleMagicsForStar(content, *role, star);
            REQUIRE_FALSE(magics.empty());
            const int ultimateId = magics.back().first->ID;
            if (roleUltimateId >= 0) CHECK(ultimateId == roleUltimateId);
            roleUltimateId = ultimateId;
        }
        result.insert(roleUltimateId);
    }
    return result;
}

}  // namespace KysChess::Test
