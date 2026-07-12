#include "ChessStandaloneBattle.h"

#include "BattleSetupFactory.h"
#include "ChessBattleMapCatalog.h"

#include <algorithm>
#include <climits>
#include <format>
#include <tuple>

namespace KysChess
{
namespace
{

RoleSave classicRoleDefinition(RoleSave role)
{
    std::vector<std::tuple<int, int, int>> learned;
    for (int index = 0; index < ROLE_MAGIC_COUNT; ++index)
    {
        if (role.MagicID[index] > 0)
        {
            learned.emplace_back(role.MagicPower[index], role.MagicID[index], index);
        }
        role.MagicID[index] = 0;
        role.MagicPower[index] = 0;
    }
    std::ranges::sort(learned);
    if (!learned.empty())
    {
        role.MagicPower[0] = std::get<0>(learned.front());
        role.MagicID[0] = std::get<1>(learned.front());
    }
    if (learned.size() > 1)
    {
        role.MagicPower[1] = std::get<0>(learned.back());
        role.MagicID[1] = std::get<1>(learned.back());
    }
    return role;
}

std::shared_ptr<const ChessGameContent> contentForRequest(
    const ChessGameContent& source,
    const ChessStandaloneBattleRequest& request)
{
    if (request.roleOverrides.empty()
        && request.profile == ChessStandaloneBattleProfile::AutoChess)
    {
        return {};
    }

    ChessGameContentData data;
    data.difficulty = source.difficulty();
    data.balance = source.balance();
    data.roles = source.roles();
    data.magics = source.magics();
    data.items = source.items();
    data.poolRoleIds = source.poolRoleIds();
    data.combos = source.combos();
    data.equipment = source.equipment();
    data.equipmentSynergies = source.equipmentSynergies();
    data.neigongConfig = source.neigongConfig();
    data.neigong = source.neigong();
    data.magicEffects = source.magicEffects();
    data.battleMaps = source.battleMaps();
    data.battlefields = source.battlefields();

    for (const auto& [roleId, sourceRole] : request.roleOverrides)
    {
        ChessRoleDefinition role;
        static_cast<RoleSave&>(role) = request.profile == ChessStandaloneBattleProfile::ClassicHades
            ? classicRoleDefinition(sourceRole)
            : sourceRole;
        role.ID = roleId;
        data.roles[roleId] = std::move(role);
    }

    if (request.profile == ChessStandaloneBattleProfile::ClassicHades)
    {
        data.combos.clear();
        data.equipment.clear();
        data.equipmentSynergies.clear();
        data.neigongConfig = {};
        data.neigong.clear();
        data.magicEffects.clear();
    }
    return std::make_shared<const ChessGameContent>(std::move(data), source.gameVersion());
}

bool mapCanFit(
    const ChessGameContent& content,
    int mapId,
    int allyCount,
    int enemyCount)
{
    const auto found = content.battleMaps().find(mapId);
    if (found == content.battleMaps().end())
    {
        return false;
    }
    const auto& map = found->second;
    int allyCapacity = std::min(map.teammateX.size(), map.teammateY.size());
    int enemyCapacity = std::min(map.enemyX.size(), map.enemyY.size());
    if (const auto* catalog = ChessBattleMapCatalog::find(mapId))
    {
        allyCapacity = static_cast<int>(catalog->teammatePositions.size());
        enemyCapacity = std::min(enemyCapacity, catalog->enemyCapacity);
    }
    return allyCount <= allyCapacity && enemyCount <= enemyCapacity;
}

bool validatePiece(
    const ChessStandaloneBattlePiece& piece,
    const ChessGameContent& content,
    std::string_view team,
    int index,
    std::string& error)
{
    if (!content.role(piece.roleId))
    {
        error = std::format("{}第{}名角色 ID {} 不存在", team, index + 1, piece.roleId);
        return false;
    }
    if (piece.star < 1 || piece.star > 3)
    {
        error = std::format("{}第{}名角色星級 {} 無效", team, index + 1, piece.star);
        return false;
    }
    for (const int itemId : {piece.weaponItemId, piece.armorItemId})
    {
        if (itemId >= 0 && !content.item(itemId))
        {
            error = std::format("{}第{}名角色裝備 ID {} 不存在", team, index + 1, itemId);
            return false;
        }
    }
    return true;
}

void appendTeam(
    PreparedChessBattle& prepared,
    const std::vector<ChessStandaloneBattlePiece>& pieces,
    int team)
{
    for (const auto& piece : pieces)
    {
        prepared.units.push_back({
            static_cast<int>(prepared.units.size()) + 1,
            piece.chessInstanceId,
            piece.roleId,
            team,
            piece.star,
            piece.weaponItemId,
            piece.armorItemId,
            piece.fightsWon,
        });
    }
}

}

std::unique_ptr<ChessGameSession> ChessStandaloneBattleBuild::createSession() &&
{
    return ChessGameSession::createStandaloneBattle(
        std::move(content),
        rootSeed,
        std::move(preparedBattle),
        std::move(obtainedNeigongIds),
        options);
}

std::optional<ChessStandaloneBattleBuild> ChessStandaloneBattle::prepare(
    std::shared_ptr<const ChessGameContent> content,
    const ChessStandaloneBattleRequest& request,
    std::string& error)
{
    error.clear();
    if (!content)
    {
        error = "沒有可用的自走棋規則內容";
        return std::nullopt;
    }
    if (request.allies.empty() || request.enemies.empty())
    {
        error = "獨立戰鬥的雙方陣容都不可為空";
        return std::nullopt;
    }

    if (auto replacement = contentForRequest(*content, request))
    {
        content = std::move(replacement);
    }
    for (int index = 0; index < static_cast<int>(request.allies.size()); ++index)
    {
        if (!validatePiece(request.allies[index], *content, "我方", index, error))
        {
            return std::nullopt;
        }
    }
    for (int index = 0; index < static_cast<int>(request.enemies.size()); ++index)
    {
        if (!validatePiece(request.enemies[index], *content, "敵方", index, error))
        {
            return std::nullopt;
        }
    }

    ChessRunRandom random(request.rootSeed);
    PreparedChessBattle prepared;
    prepared.kind = PreparedChessBattleKind::Standalone;
    prepared.stableBattleId = request.stableBattleId.empty()
        ? "standalone"
        : request.stableBattleId;
    prepared.preparationCheckpoint = random.checkpointPreparation();
    appendTeam(prepared, request.allies, 0);
    appendTeam(prepared, request.enemies, 1);

    const int allyCount = static_cast<int>(request.allies.size());
    const int enemyCount = static_cast<int>(request.enemies.size());
    if (request.mapId)
    {
        if (!mapCanFit(*content, *request.mapId, allyCount, enemyCount))
        {
            error = std::format("戰場 ID {} 不存在或無法容納雙方陣容", *request.mapId);
            return std::nullopt;
        }
        prepared.mapCandidates = {*request.mapId};
        prepared.chosenMapId = *request.mapId;
    }
    else
    {
        for (const int mapId : ChessBattleMapCatalog::fittingMapIds(
                 *content,
                 allyCount,
                 enemyCount))
        {
            if (mapCanFit(*content, mapId, allyCount, enemyCount))
            {
                prepared.mapCandidates.push_back(mapId);
            }
        }
        if (prepared.mapCandidates.empty())
        {
            error = "沒有可容納目前陣容的戰場";
            return std::nullopt;
        }
        prepared.chosenMapId = prepared.mapCandidates[random.nextInt(
            ChessRngStream::MapSelection,
            static_cast<int>(prepared.mapCandidates.size()))];
    }
    prepared.battleSeed = request.battleSeed
        ? *request.battleSeed
        : static_cast<std::uint32_t>(random.nextBounded(
            ChessRngStream::BattleSeed,
            static_cast<std::uint64_t>(UINT_MAX) + 1));
    BattleSetupFactory::populateBaseFormation(prepared, *content);

    ChessStandaloneBattleBuild result;
    result.content = std::move(content);
    result.preparedBattle = std::move(prepared);
    result.obtainedNeigongIds = request.profile == ChessStandaloneBattleProfile::ClassicHades
        ? std::set<int>{}
        : request.obtainedNeigongIds;
    result.options = request.options;
    result.rootSeed = request.rootSeed;
    return result;
}

}
