#include "ChessGameContent.h"

#include <algorithm>
#include <tuple>

namespace KysChess
{

ChessGameContent::ChessGameContent(ChessGameContentData data, std::string gameVersion)
    : data_(std::make_shared<const ChessGameContentData>(std::move(data))),
      gameVersion_(std::move(gameVersion))
{
}

const ChessRoleDefinition* ChessGameContent::role(int roleId) const
{
    const auto found = data_->roles.find(roleId);
    return found == data_->roles.end() ? nullptr : &found->second;
}

const ChessMagicDefinition* ChessGameContent::magic(int magicId) const
{
    const auto found = data_->magics.find(magicId);
    return found == data_->magics.end() ? nullptr : &found->second;
}

const ChessItemDefinition* ChessGameContent::item(int itemId) const
{
    const auto found = data_->items.find(itemId);
    return found == data_->items.end() ? nullptr : &found->second;
}

std::vector<std::pair<const ChessMagicDefinition*, int>> chessRoleMagicsForStar(
    const ChessGameContent& content,
    const ChessRoleDefinition& role,
    int star)
{
    std::vector<std::pair<const ChessMagicDefinition*, int>> result;
    for (int index = RoleSave::getMagicSlotStart(star);
         index < RoleSave::getMagicSlotEnd(star);
         ++index)
    {
        if (const auto* magic = content.magic(role.MagicID[index]))
        {
            result.emplace_back(magic, role.MagicPower[index]);
        }
    }
    std::ranges::sort(result, [](const auto& lhs, const auto& rhs) {
        return std::tuple{lhs.second, lhs.first->ID}
            < std::tuple{rhs.second, rhs.first->ID};
    });
    return result;
}

}
