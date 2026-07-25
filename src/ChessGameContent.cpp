#include "ChessGameContent.h"

#include <algorithm>
#include <array>
#include <tuple>

namespace KysChess
{
namespace
{

auto roleContentView(const ChessRoleDefinition& role)
{
    std::array<int, ROLE_MAGIC_COUNT> magicIds{};
    std::array<int, ROLE_MAGIC_COUNT> magicPower{};
    std::ranges::copy(role.MagicID, magicIds.begin());
    std::ranges::copy(role.MagicPower, magicPower.begin());
    return std::tuple{
        role.ID,
        role.HeadID,
        role.Cost,
        role.Sexual,
        role.MaxHP,
        role.MPType,
        role.MaxMP,
        role.Attack,
        role.Speed,
        role.Defence,
        role.Medicine,
        role.UsePoison,
        role.Detoxification,
        role.AntiPoison,
        role.Fist,
        role.Sword,
        role.Knife,
        role.Unusual,
        role.HiddenWeapon,
        role.Knowledge,
        role.Morality,
        role.AttackWithPoison,
        role.AttackTwice,
        role.Fame,
        role.IQ,
        std::move(magicIds),
        std::move(magicPower),
        role.Name,
        role.Nick,
    };
}

auto magicContentView(const ChessMagicDefinition& magic)
{
    return std::tuple{
        magic.ID,
        magic.SoundID,
        magic.MagicType,
        magic.EffectID,
        magic.HurtType,
        magic.AttackAreaType,
        magic.NeedMP,
        magic.WithPoison,
        magic.SelectDistance,
        magic.AttackDistance,
        magic.AddMP,
        magic.HurtMP,
        magic.Name,
    };
}

auto roleContentViews(const std::map<int, ChessRoleDefinition>& roles)
{
    using View = decltype(roleContentView(std::declval<const ChessRoleDefinition&>()));
    std::vector<std::pair<int, View>> result;
    result.reserve(roles.size());
    for (const auto& [id, role] : roles)
    {
        result.emplace_back(id, roleContentView(role));
    }
    return result;
}

auto magicContentViews(const std::map<int, ChessMagicDefinition>& magics)
{
    using View = decltype(magicContentView(std::declval<const ChessMagicDefinition&>()));
    std::vector<std::pair<int, View>> result;
    result.reserve(magics.size());
    for (const auto& [id, magic] : magics)
    {
        result.emplace_back(id, magicContentView(magic));
    }
    return result;
}

ChessSha256 chessContentFingerprint(const ChessGameContentData& data)
{
    const auto roles = roleContentViews(data.roles);
    const auto magics = magicContentViews(data.magics);
    return chessBeveSha256(
        "KYS_CHESS_CONTENT",
        data.difficulty,
        data.balance,
        roles,
        magics,
        data.items,
        data.poolRoleIds,
        data.combos,
        data.equipment,
        data.equipmentSynergies,
        data.neigongConfig,
        data.neigong,
        data.magicEffects,
        data.battleMaps,
        data.battlefields);
}

}

ChessGameContent::ChessGameContent(ChessGameContentData data, std::string gameVersion)
    : data_(std::make_shared<const ChessGameContentData>(std::move(data))),
      contentFingerprint_(chessContentFingerprint(*data_)),
      gameVersion_(std::move(gameVersion))
{
}

std::shared_ptr<const ChessGameContent> ChessGameContent::withGameVersion(
    std::string gameVersion) const
{
    auto result = std::make_shared<ChessGameContent>(*this);
    result->gameVersion_ = std::move(gameVersion);
    return result;
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
