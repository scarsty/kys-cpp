#include "BattleSceneMapState.h"

#include "BattleMap.h"
#include "Engine.h"
#include "GameUtil.h"
#include "Scene.h"
#include "TextureManager.h"

#include <algorithm>
#include <cassert>
#include <cmath>

void BattleSceneMapState::initialize(int coordCount)
{
    assert(coordCount > 0);
    coordCount_ = coordCount;
    earthLayer_.resize(coordCount_);
    buildingLayer_.resize(coordCount_);
}

void BattleSceneMapState::loadBattlefield(int battlefieldId)
{
    assert(coordCount_ > 0);
    battlefieldId_ = battlefieldId;
    BattleMap::getInstance()->copyLayerData(battlefieldId_, 0, &earthLayer_);
    BattleMap::getInstance()->copyLayerData(battlefieldId_, 1, &buildingLayer_);
}

bool BattleSceneMapState::isOutLine(int x, int y)
{
    return x < 0 || x >= coordCount_ || y < 0 || y >= coordCount_;
}

bool BattleSceneMapState::isBuilding(int x, int y)
{
    return buildingLayer_.data(x, y) > 0;
}

bool BattleSceneMapState::isWater(int x, int y)
{
    const int num = earthLayer_.data(x, y) / 2;
    return (num >= 179 && num <= 181)
        || num == 261
        || num == 511
        || (num >= 662 && num <= 665)
        || num == 674;
}

bool BattleSceneMapState::canWalk45(int x, int y)
{
    return !isOutLine(x, y) && !isBuilding(x, y) && !isWater(x, y);
}

Pointf BattleSceneMapState::pos45To90(int x, int y) const
{
    Pointf p;
    p.x = static_cast<float>(-y * Scene::TILE_W + x * Scene::TILE_W + coordCount_ * Scene::TILE_W);
    p.y = static_cast<float>(y * Scene::TILE_W + x * Scene::TILE_W);
    return p;
}

Point BattleSceneMapState::pos90To45(double x, double y) const
{
    x -= coordCount_ * Scene::TILE_W;
    Point p;
    p.x = static_cast<int>(std::round((x / Scene::TILE_W + y / Scene::TILE_W) / 2));
    p.y = static_cast<int>(std::round((-x / Scene::TILE_W + y / Scene::TILE_W) / 2));
    return p;
}

Point BattleSceneMapState::positionOnWholeEarth(int x, int y, int renderCenterX, int renderCenterY) const
{
    Point p;
    p.x = -y * Scene::TILE_W + x * Scene::TILE_W + renderCenterX;
    p.y = y * Scene::TILE_H + x * Scene::TILE_H + renderCenterY;
    p.x += coordCount_ * Scene::TILE_W - renderCenterX;
    p.y += 2 * Scene::TILE_H - renderCenterY;
    return p;
}

void BattleSceneMapState::makeEarthTexture(int renderCenterX, int renderCenterY)
{
    assert(coordCount_ > 0);
    assert(battlefieldId_ >= 0);

    if (GameUtil::isLegacyBrowser())
    {
        Engine::getInstance()->destroyTexture("earth");
        return;
    }

    Engine::getInstance()->createRenderedTexture(
        "earth",
        coordCount_ * Scene::TILE_W * 2,
        coordCount_ * Scene::TILE_H * 2);
    Engine::getInstance()->setRenderTarget("earth");
    Engine::getInstance()->fillColor(
        { 0, 0, 0, 255 },
        0,
        0,
        coordCount_ * Scene::TILE_W * 2,
        coordCount_ * Scene::TILE_H * 2);

    for (int x = 0; x < coordCount_; ++x)
    {
        for (int y = 0; y < coordCount_; ++y)
        {
            const auto position = positionOnWholeEarth(x, y, renderCenterX, renderCenterY);
            const int num = earthLayer_.data(x, y) / 2;
            if (num > 0)
            {
                TextureManager::getInstance()->renderTexture("smap", num, position.x, position.y);
            }
        }
    }

    if (TextureManager::getInstance()->getTextureGroup("battle-earth")->getTextureCount() > 0)
    {
        TextureManager::getInstance()->renderTexture("battle-earth", battlefieldId_, 0, 0);
    }

    Engine::getInstance()->resetRenderTarget();
}
