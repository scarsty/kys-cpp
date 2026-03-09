#include "ChessSelectorPresenter.h"

#include "ChessEquipment.h"
#include "ChessManager.h"
#include "ChessPool.h"
#include "Font.h"
#include "Save.h"

namespace KysChess
{

int ChessSelectorPresenter::getDisplayWidth(const std::string& text) const
{
    return Font::getTextDrawSize(text);
}

std::pair<std::string, Color> ChessSelectorPresenter::formatChessName(Role* role,
    int tier,
    std::optional<int> starOpt,
    std::optional<int> countOpt,
    const std::string& prefix) const
{
    std::string result;
    int displayWidth = 0;

    if (!prefix.empty())
    {
        result += prefix;
        displayWidth += getDisplayWidth(prefix);
    }

    result += role->Name;
    displayWidth += getDisplayWidth(role->Name);
    while (displayWidth < 16)
    {
        result += " ";
        displayWidth += 1;
    }

    int starWidth = 0;
    if (starOpt)
    {
        for (int i = 0; i < *starOpt; ++i)
        {
            result += "★";
            starWidth += 2;
        }
    }
    while (starWidth < 8)
    {
        result += " ";
        starWidth += 1;
    }

    int quantityWidth = 0;
    if (countOpt)
    {
        std::string countStr = "x" + std::to_string(*countOpt);
        result += countStr;
        quantityWidth += static_cast<int>(countStr.size());
    }
    while (quantityWidth < 6)
    {
        result += " ";
        quantityWidth += 1;
    }

    std::string costStr = "$" + std::to_string(ChessManager::calculateCost(tier, starOpt.value_or(1), countOpt.value_or(1)));
    while (static_cast<int>(costStr.size()) < 6)
    {
        costStr = " " + costStr;
    }
    result += costStr;

    return {result, ChessPool::GetTierColor(tier)};
}

ChessMenuData ChessSelectorPresenter::buildChessMenuData(const std::vector<ChessMenuEntry>& entries) const
{
    ChessMenuData data;
    data.labels.reserve(entries.size());
    data.colors.reserve(entries.size());
    data.previewData.reserve(entries.size());
    for (const auto& entry : entries)
    {
        int tier = ChessPool::GetChessTier(entry.chess.role->ID);
        auto [label, color] = formatChessName(entry.chess.role, tier, entry.chess.star, {}, entry.prefix);
        data.labels.push_back(label);
        data.colors.push_back(color);
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