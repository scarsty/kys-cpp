#include "BattleSceneSetupBuilder.h"

#include "BattleSceneRenderMath.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace KysChess::BattleSceneSetupBuilder
{
namespace
{
int normalizeMagicStar(int star)
{
    const int maxStar = ROLE_MAGIC_COUNT / SLOTS_PER_STAR;
    if (star < 1)
    {
        return 1;
    }
    if (star > maxStar)
    {
        return maxStar;
    }
    return star;
}

int magicSlotStart(int star)
{
    return (normalizeMagicStar(star) - 1) * SLOTS_PER_STAR;
}

int magicSlotEnd(int star)
{
    return (std::min)(magicSlotStart(star) + SLOTS_PER_STAR, static_cast<int>(ROLE_MAGIC_COUNT));
}

int actPropertyForMagicType(const Role& role, int type)
{
    switch (type)
    {
    case 0: return role.Medicine;
    case 1: return role.Fist;
    case 2: return role.Sword;
    case 3: return role.Knife;
    case 4:
    case 5:
    case 6:
    case 7:
        return role.Unusual;
    default:
        return 0;
    }
}

std::vector<Magic*> learnedMagicsForStar(const Role& role, int star, const std::function<Magic*(int)>& magicById)
{
    assert(magicById);
    std::vector<Magic*> magics;
    for (int index = magicSlotStart(star); index < magicSlotEnd(star); ++index)
    {
        auto* magic = magicById(role.MagicID[index]);
        if (magic)
        {
            magics.push_back(magic);
        }
    }
    return magics;
}

int magicPowerForStar(const Role& role, int magicId, int star)
{
    for (int index = magicSlotStart(star); index < magicSlotEnd(star); ++index)
    {
        if (role.MagicID[index] == magicId)
        {
            return role.MagicPower[index];
        }
    }
    return 0;
}

template<typename Cmp>
Magic* selectBattleMagic(
    const Role& role,
    int star,
    const std::function<Magic*(int)>& magicById,
    Cmp cmp)
{
    auto learned = learnedMagicsForStar(role, star, magicById);
    if (learned.empty())
    {
        return nullptr;
    }

    Magic* chosen = learned.front();
    int chosenPower = magicPowerForStar(role, chosen->ID, star);
    for (std::size_t index = 1; index < learned.size(); ++index)
    {
        const int candidatePower = magicPowerForStar(role, learned[index]->ID, star);
        if (cmp(candidatePower, chosenPower))
        {
            chosen = learned[index];
            chosenPower = candidatePower;
        }
    }
    return chosen;
}

KysChess::BattleSceneBattleAdapter::BattleSetupSkillSnapshot makeSkillSnapshot(
    const Role& role,
    int star,
    Magic* magic)
{
    KysChess::BattleSceneBattleAdapter::BattleSetupSkillSnapshot snapshot;
    if (!magic)
    {
        return snapshot;
    }

    snapshot.id = magic->ID;
    snapshot.name = magic->Name;
    snapshot.soundId = magic->SoundID;
    snapshot.hurtType = magic->HurtType;
    snapshot.attackAreaType = magic->AttackAreaType;
    snapshot.magicType = magic->MagicType;
    snapshot.visualEffectId = magic->EffectID;
    snapshot.selectDistance = magic->SelectDistance;
    snapshot.actProperty = actPropertyForMagicType(role, magic->MagicType);
    snapshot.magicPower = magicPowerForStar(role, magic->ID, star);
    return snapshot;
}

std::string skillNamesForStar(
    const Role& role,
    int star,
    const std::function<Magic*(int)>& magicById)
{
    std::string names;
    for (auto* magic : learnedMagicsForStar(role, star, magicById))
    {
        if (!names.empty())
        {
            names += " ";
        }
        names += magic->Name;
    }
    return names;
}

int setupDistance(int x1, int y1, int x2, int y2)
{
    return std::abs(x1 - x2) + std::abs(y1 - y2);
}

int setupTowards(int x1, int y1, int x2, int y2)
{
    const int dy = y2 - y1;
    const int dx = x2 - x1;
    const int deltaMagnitude = std::abs(dy) - std::abs(dx);
    if (dy == 0 && dx == 0)
    {
        return Towards_None;
    }
    if (deltaMagnitude >= 0)
    {
        return dy < 0 ? Towards_RightUp : Towards_LeftDown;
    }
    return dx < 0 ? Towards_LeftUp : Towards_RightDown;
}

int facingTowardNearestOpponent(
    const BattleSceneSetupUnitRequest& request,
    std::span<const BattleSceneSetupOpponentCell> opponents)
{
    int faceTowards = request.faceTowardsFallback;
    int minDistance = std::numeric_limits<int>::max();
    for (const auto& opponent : opponents)
    {
        if (opponent.team == request.team)
        {
            continue;
        }
        const int distance = setupDistance(request.gridX, request.gridY, opponent.x, opponent.y);
        if (distance < minDistance)
        {
            minDistance = distance;
            const int candidate = setupTowards(request.gridX, request.gridY, opponent.x, opponent.y);
            if (candidate != Towards_None)
            {
                faceTowards = candidate;
            }
        }
    }
    return faceTowards;
}
}  // namespace

KysChess::BattleSceneBattleAdapter::BattleSetupUnitInput makeSetupUnit(
    const BattleSceneSetupUnitRequest& request,
    std::span<const BattleSceneSetupOpponentCell> opponents)
{
    assert(request.source);
    assert(request.unitId >= 0);
    assert(request.team == 0 || request.team == 1);
    assert(request.positionForCell);
    assert(request.fightFramesForHeadId);
    assert(request.magicById);

    const auto& role = *request.source;
    const int faceTowards = facingTowardNearestOpponent(request, opponents);

    KysChess::BattleSceneBattleAdapter::BattleSetupUnitInput unit;
    unit.unitId = request.unitId;
    unit.realRoleId = role.RealID >= 0 ? role.RealID : role.ID;
    unit.name = role.Name;
    unit.headId = role.HeadID;
    unit.team = request.team;
    unit.sourceOrder = request.sourceOrder;
    unit.alive = true;
    unit.gridX = request.gridX;
    unit.gridY = request.gridY;
    unit.faceTowards = faceTowards;
    const Pointf position = request.positionForCell(request.gridX, request.gridY);
    const Pointf velocity{};
    const Pointf acceleration{ 0, 0, request.gravity };
    const Pointf facing = BattleSceneRenderMath::realTowardsFromFaceTowards(faceTowards);
    unit.motion = { position, velocity, acceleration, facing };
    unit.fightFrames = request.fightFramesForHeadId(role.HeadID);
    unit.star = request.star;
    unit.cost = role.Cost;
    unit.weaponId = request.weaponId;
    unit.armorId = request.armorId;
    unit.chessInstanceId = request.chessInstanceId;
    unit.fightsWon = request.fightsWon;
    unit.vitals = { role.MaxHP, role.MaxHP, 0, role.MaxMP };
    unit.stats = { role.Attack, role.Defence, role.Speed };
    unit.fist = role.Fist;
    unit.sword = role.Sword;
    unit.knife = role.Knife;
    unit.unusual = role.Unusual;
    unit.hiddenWeapon = role.HiddenWeapon;
    unit.animation = { 0, 0, 0, -1 };
    unit.physicalPower = (std::max)(role.PhysicalPower, 90);
    unit.invincible = role.Invincible;
    unit.hurtFrame = role.HurtFrame;
    unit.frozen = role.Frozen;
    unit.frozenMax = role.FrozenMax;
    for (int magicType = 0; magicType < BattleSceneRenderMath::FightStyleCount; ++magicType)
    {
        unit.actPropertiesByMagicType[magicType] = actPropertyForMagicType(role, magicType);
    }

    Magic* normalMagic = role.UsingMagic;
    Magic* ultimateMagic = role.UsingMagic;
    unit.hasEquippedSkill = normalMagic != nullptr;
    if (!normalMagic)
    {
        normalMagic = selectBattleMagic(
            role,
            request.star,
            request.magicById,
            [](int candidate, int current)
            {
                return candidate < current;
            });
        ultimateMagic = selectBattleMagic(
            role,
            request.star,
            request.magicById,
            [](int candidate, int current)
            {
                return candidate > current;
            });
    }
    unit.normalSkill = makeSkillSnapshot(role, request.star, normalMagic);
    unit.ultimateSkill = makeSkillSnapshot(role, request.star, ultimateMagic);
    unit.skillNames = skillNamesForStar(role, request.star, request.magicById);
    return unit;
}

BattleSceneSetupBuildResult buildSetupUnits(std::span<const BattleSceneSetupUnitRequest> requests)
{
    std::vector<BattleSceneSetupOpponentCell> cells;
    cells.reserve(requests.size());
    for (const auto& request : requests)
    {
        cells.push_back({ request.team, request.gridX, request.gridY });
    }

    BattleSceneSetupBuildResult result;
    result.units.reserve(requests.size());
    for (const auto& request : requests)
    {
        result.units.push_back(makeSetupUnit(request, cells));
    }
    std::ranges::sort(
        result.units,
        [](const auto& lhs, const auto& rhs)
        {
            return lhs.unitId < rhs.unitId;
        });
    for (std::size_t index = 0; index < result.units.size(); ++index)
    {
        assert(result.units[index].unitId == static_cast<int>(index));
    }
    return result;
}

}  // namespace KysChess::BattleSceneSetupBuilder
