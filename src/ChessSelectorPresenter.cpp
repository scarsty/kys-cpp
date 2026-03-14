#include "ChessSelectorPresenter.h"

#include "ChessEquipment.h"
#include "ChessManager.h"
#include "ChessPool.h"
#include "Font.h"
#include "Save.h"
#include "ChessScreenLayout.h"

namespace KysChess
{

int ChessSelectorPresenter::getDisplayWidth(const std::string& text) const
{
    return Font::getTextDrawSize(text);
}

ChessSelectorPresenter::FormattedChessName ChessSelectorPresenter::formatChessName(Role* role,
    int tier,
    std::optional<int> starOpt,
    const std::string& prefix) const
{
    // Use the shared menu-budget constants so anchors and formatter agree.

    std::string result;

    int prefixUnits = prefix.empty() ? 0 : getDisplayWidth(prefix);
    int nameUnitsActual = prefixUnits + getDisplayWidth(role->Name);
    int nameUnits = std::max(ChessScreenLayout::kMenuNameUnits, nameUnitsActual);

    // Append prefix and name
    if (!prefix.empty())
    {
        result += prefix;
    }
    result += role->Name;

    // Pad name region to target units
    int padAfterName = nameUnits - nameUnitsActual;
    for (int i = 0; i < padAfterName; ++i) result += ' ';

    // Stars region
    int starUnitsActual = 0;
    if (starOpt)
    {
        for (int i = 0; i < *starOpt; ++i)
        {
            result += "★";
            starUnitsActual += getDisplayWidth("★");
        }
    }
    int starUnits = std::max(ChessScreenLayout::kMenuStarUnits, starUnitsActual);
    int padStars = starUnits - starUnitsActual;
    for (int i = 0; i < padStars; ++i) result += ' ';

    // Cost region: right-pad to a small fixed width so columns align
    std::string costStr = "$" + std::to_string(ChessManager::calculateCost(tier, starOpt.value_or(1), 1));
    while (getDisplayWidth(costStr) < ChessScreenLayout::kMenuCostUnits)
    {
        costStr = " " + costStr;
    }
    result += costStr;

    int totalUnits = nameUnits + starUnits + getDisplayWidth(costStr);

    return {result, ChessPool::GetTierColor(tier), totalUnits};
}

ChessMenuData ChessSelectorPresenter::buildChessMenuData(const std::vector<ChessMenuEntry>& entries) const
{
    ChessMenuData data;
    data.labels.reserve(entries.size());
    data.colors.reserve(entries.size());
    data.previewData.reserve(entries.size());
    for (const auto& entry : entries)
    {
        int tier = entry.chess.role ? entry.chess.role->Cost : -1;
        auto formatted = formatChessName(entry.chess.role, tier, entry.chess.star, entry.prefix);
        data.labels.push_back(formatted.text);
        data.colors.push_back(formatted.color);
        data.previewData.push_back(entry.chess);
    }
    return data;
}

std::string ChessSelectorPresenter::challengeRewardDesc(const BalanceConfig::ChallengeReward& reward) const
{
    using RT = BalanceConfig::ChallengeRewardType;
    switch (reward.type)
    {
    case RT::Gold: return std::format("獲取{}金幣", reward.value);
    case RT::GetPiece: return std::format("獲取棋子(最高{}費)", reward.value);
    case RT::GetNeigong: return std::format("獲取內功(最高{}階)", reward.value);
    case RT::StarUp1to2: return std::format("升星★→★★(最高{}費)", reward.value);
    case RT::StarUp2to3: return std::format("升星★★→★★★(最高{}費)", reward.value);
    case RT::GetEquipment: return std::format("獲取裝備(最高{}階)", reward.value);
    case RT::GetSpecificEquipment:
        if (auto* item = Save::getInstance()->getItem(reward.value))
            return std::format("獲取指定裝備: {}", item->Name);
        return "獲取指定裝備";
    }
    return "未知獎勵";
}

std::string ChessSelectorPresenter::buildChessNameWithStar(const Chess& chess) const
{
    std::string name = chess.role ? std::string(chess.role->Name) : std::string("???");
    for (int i = 0; i < chess.star; ++i)
    {
        name += "★";
    }
    return name;
}

std::vector<std::string> ChessSelectorPresenter::buildEquippedBy(const std::map<ChessInstanceID, Chess>& collection, int itemId) const
{
    std::vector<std::string> equippedBy;
    for (const auto& [instanceId, chess] : collection)
    {
        if (chess.weaponInstance.itemId == itemId)
        {
            equippedBy.push_back(buildChessNameWithStar(chess));
        }
        if (chess.armorInstance.itemId == itemId)
        {
            equippedBy.push_back(buildChessNameWithStar(chess));
        }
    }
    return equippedBy;
}

}