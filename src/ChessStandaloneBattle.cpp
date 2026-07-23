#include "ChessStandaloneBattle.h"

#include "BattleSetupFactory.h"
#include "ChessBattleMapCatalog.h"
#include "ChessPvp.h"

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
    int allyRequiredSlots,
    int enemyRequiredSlots)
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
    return allyRequiredSlots <= allyCapacity && enemyRequiredSlots <= enemyCapacity;
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
    const ChessStandaloneBattleTeam& source,
    int team)
{
    for (int index = 0; index < static_cast<int>(source.pieces.size()); ++index)
    {
        const auto& piece = source.pieces[index];
        PreparedChessBattleUnit unit;
        unit.unitId = static_cast<int>(prepared.units.size()) + 1;
        unit.chessInstanceId = piece.chessInstanceId;
        unit.roleId = piece.roleId;
        unit.team = team;
        unit.star = piece.star;
        unit.weaponItemId = piece.weaponItemId;
        unit.armorItemId = piece.armorItemId;
        unit.fightsWon = piece.fightsWon;
        if (source.formationSlots.empty())
        {
            unit.formationSlot = index;
        }
        else
        {
            const auto slot = std::ranges::find(source.formationSlots, piece.chessInstanceId);
            assert(slot != source.formationSlots.end());
            unit.formationSlot = static_cast<int>(slot - source.formationSlots.begin());
        }
        prepared.units.push_back(std::move(unit));
    }
}

bool validateTeamFormation(
    const ChessStandaloneBattleTeam& team,
    std::string_view name,
    bool limitSequentialFormation,
    std::string& error)
{
    if (team.formationSlots.empty())
    {
        if (limitSequentialFormation && team.pieces.size() > kChessFormationSlotCount)
        {
            error = std::format("{}陣容超過十名棋子", name);
            return false;
        }
        return true;
    }
    if (team.formationSlots.size() != kChessFormationSlotCount)
    {
        error = std::format("{}陣形必須包含十格", name);
        return false;
    }
    std::set<int> pieceIds;
    for (const auto& piece : team.pieces)
    {
        if (piece.chessInstanceId < 0 || !pieceIds.insert(piece.chessInstanceId).second)
        {
            error = std::format("{}陣形需要唯一的棋子實例 ID", name);
            return false;
        }
    }
    std::set<int> placed;
    for (const int id : team.formationSlots)
    {
        if (id == -1)
        {
            continue;
        }
        if (id < 0 || !pieceIds.contains(id) || !placed.insert(id).second)
        {
            error = std::format("{}陣形包含無效或重複的棋子實例 ID", name);
            return false;
        }
    }
    if (placed != pieceIds)
    {
        error = std::format("{}陣形沒有完整放置所有棋子", name);
        return false;
    }
    return true;
}

}

ChessStandaloneBattleTeam chessStandaloneBattleTeam(
    const ChessPvpComposition& composition)
{
    ChessStandaloneBattleTeam result;
    result.formationSlots = composition.formationSlots;
    result.obtainedNeigongIds = composition.obtainedNeigongIds;
    for (const auto& piece : composition.pieces)
    {
        result.pieces.push_back({
            piece.roleId,
            piece.star,
            piece.weaponItemId,
            piece.armorItemId,
            piece.chessInstanceId,
            piece.fightsWon,
        });
    }
    return result;
}

std::unique_ptr<ChessGameSession> ChessStandaloneBattleBuild::createSession() &&
{
    return ChessGameSession::createStandaloneBattle(
        std::move(content),
        rootSeed,
        std::move(preparedBattle),
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
    if (request.teams[0].pieces.empty() || request.teams[1].pieces.empty())
    {
        error = "獨立戰鬥的雙方陣容都不可為空";
        return std::nullopt;
    }

    if (auto replacement = contentForRequest(*content, request))
    {
        content = std::move(replacement);
    }
    for (int team = 0; team < 2; ++team)
    {
        const std::string_view teamName = team == 0 ? "我方" : "敵方";
        if (!validateTeamFormation(request.teams[team], teamName, team == 0, error))
        {
            return std::nullopt;
        }
        for (int index = 0; index < static_cast<int>(request.teams[team].pieces.size()); ++index)
        {
            if (!validatePiece(request.teams[team].pieces[index], *content, teamName, index, error))
            {
                return std::nullopt;
            }
        }
    }

    ChessRunRandom random(request.rootSeed);
    PreparedChessBattle prepared;
    prepared.kind = PreparedChessBattleKind::Standalone;
    prepared.layout = request.layout;
    prepared.stableBattleId = request.stableBattleId.empty()
        ? "standalone"
        : request.stableBattleId;
    prepared.preparationCheckpoint = random.checkpointPreparation();
    appendTeam(prepared, request.teams[0], 0);
    appendTeam(prepared, request.teams[1], 1);
    if (request.profile != ChessStandaloneBattleProfile::ClassicHades)
    {
        for (int team = 0; team < 2; ++team)
        {
            prepared.obtainedNeigongIdsByTeam[team] = request.teams[team].obtainedNeigongIds;
        }
    }

    const int allyRequiredSlots = BattleSetupFactory::requiredFormationSlots(prepared, 0);
    const int enemyRequiredSlots = BattleSetupFactory::requiredFormationSlots(prepared, 1);
    if (request.layout == PreparedChessBattleLayout::PvpArena)
    {
        if ((request.mapId && *request.mapId != ChessPvpMapLayout::BattleId)
            || request.teams[0].formationSlots.empty()
            || request.teams[1].formationSlots.empty())
        {
            error = "PvP 競技場需要戰場 133 與雙方完整十格陣形";
            return std::nullopt;
        }
        std::string layoutError;
        if (!ChessPvpMapLayout::validate(*content, layoutError))
        {
            error = std::move(layoutError);
            return std::nullopt;
        }
        prepared.mapCandidates = {ChessPvpMapLayout::BattleId};
        prepared.chosenMapId = ChessPvpMapLayout::BattleId;
    }
    else if (request.mapId)
    {
        if (!mapCanFit(
                *content,
                *request.mapId,
                allyRequiredSlots,
                enemyRequiredSlots))
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
                 allyRequiredSlots,
                 enemyRequiredSlots))
        {
            if (mapCanFit(
                    *content,
                    mapId,
                    allyRequiredSlots,
                    enemyRequiredSlots))
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
    result.options = request.options;
    result.rootSeed = request.rootSeed;
    return result;
}

}
