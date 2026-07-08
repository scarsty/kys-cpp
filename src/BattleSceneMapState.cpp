#include "BattleSceneMapState.h"

#include "BattleMap.h"
#include "Engine.h"
#include "GameUtil.h"
#include "Scene.h"
#include "TextureManager.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace
{
constexpr float PAPER_GROUND_EXTENSION_MARGIN_RATIO = 1.0f;
constexpr int PAPER_GROUND_EDGE_STRETCH_SOURCE_DIVISOR = 16;
}

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
        Engine::getInstance()->destroyTexture("earth_base");
        Engine::getInstance()->destroyTexture("earth");
        return;
    }

    auto* engine = Engine::getInstance();
    auto* textureManager = TextureManager::getInstance();
    const int baseEarthW = coordCount_ * Scene::TILE_W * 2;
    const int baseEarthH = coordCount_ * Scene::TILE_H * 2;
    const int paperEarthW = coordCount_ * Scene::TILE_W * 2;
    const int paperEarthH = coordCount_ * Scene::TILE_W * 2;

    Texture* earthBaseTexture = nullptr;
    Texture* earthTexture = nullptr;
    int requestedBaseEarthW = baseEarthW;
    int requestedBaseEarthH = baseEarthH;
    int requestedPaperEarthW = paperEarthW;
    int requestedPaperEarthH = paperEarthH;
    int requestedGroundExtensionMargin = std::max(0, static_cast<int>(std::round(coordCount_ * PAPER_GROUND_EXTENSION_MARGIN_RATIO)));
    for (int groundExtensionMargin = requestedGroundExtensionMargin; groundExtensionMargin >= 0;)
    {
        requestedBaseEarthW = baseEarthW + groundExtensionMargin * Scene::TILE_W * 4;
        requestedBaseEarthH = baseEarthH + groundExtensionMargin * Scene::TILE_H * 4;
        requestedPaperEarthW = paperEarthW + groundExtensionMargin * Scene::TILE_W * 4;
        requestedPaperEarthH = paperEarthH + groundExtensionMargin * Scene::TILE_W * 4;
        engine->createRenderedTexture("earth_base", requestedBaseEarthW, requestedBaseEarthH);
        engine->createRenderedTexture("earth", requestedPaperEarthW, requestedPaperEarthH);
        earthBaseTexture = engine->getTexture("earth_base");
        earthTexture = engine->getTexture("earth");
        if (earthBaseTexture && earthTexture)
        {
            break;
        }
        if (groundExtensionMargin == 0)
        {
            break;
        }
        groundExtensionMargin /= 2;
    }

    if (!earthBaseTexture || !earthTexture)
    {
        engine->resetRenderTarget();
        return;
    }

    int baseEarthTextureW = requestedBaseEarthW;
    int baseEarthTextureH = requestedBaseEarthH;
    engine->getTextureSize(earthBaseTexture, baseEarthTextureW, baseEarthTextureH);
    int paperEarthTextureW = requestedPaperEarthW;
    int paperEarthTextureH = requestedPaperEarthH;
    engine->getTextureSize(earthTexture, paperEarthTextureW, paperEarthTextureH);
    const int baseEarthOffsetX = std::max(0, (baseEarthTextureW - baseEarthW) / 2);
    const int baseEarthOffsetY = std::max(0, (baseEarthTextureH - baseEarthH) / 2);

    Engine::setTextureBlendMode(earthBaseTexture);
    Engine::setTextureBlendMode(earthTexture);
    SDL_SetTextureScaleMode(earthBaseTexture, SDL_SCALEMODE_PIXELART);
    SDL_SetTextureScaleMode(earthTexture, SDL_SCALEMODE_LINEAR);
    engine->setRenderTarget(earthBaseTexture);
    engine->fillColor({ 0, 0, 0, 0 }, 0, 0, baseEarthTextureW, baseEarthTextureH, BLENDMODE_NONE);

    struct GroundTile
    {
        TextureWarpper* texture = nullptr;
        int ix = 0;
        int iy = 0;
    };
    struct GroundDrawTile
    {
        int sourceIndex = -1;
        int ix = 0;
        int iy = 0;
    };

    std::vector<GroundTile> groundTiles;
    groundTiles.reserve(coordCount_ * coordCount_);
    auto tileIndex = [&](int ix, int iy)
    {
        return ix + iy * coordCount_;
    };
    std::vector<int> groundTileIndices(coordCount_ * coordCount_, -1);
    std::vector<int> rowFirstValid(coordCount_, -1);
    std::vector<int> rowLastValid(coordCount_, -1);
    std::vector<int> columnFirstValid(coordCount_, -1);
    std::vector<int> columnLastValid(coordCount_, -1);
    for (int ix = 0; ix < coordCount_; ++ix)
    {
        for (int iy = 0; iy < coordCount_; ++iy)
        {
            const int num = earthLayer_.data(ix, iy) / 2;
            if (num <= 0)
            {
                continue;
            }
            auto tex = textureManager->getTexture("smap", num);
            if (!tex)
            {
                continue;
            }
            const int sourceIndex = static_cast<int>(groundTiles.size());
            groundTiles.push_back({ tex, ix, iy });
            groundTileIndices[tileIndex(ix, iy)] = sourceIndex;
            if (rowFirstValid[iy] < 0 || ix < rowFirstValid[iy])
            {
                rowFirstValid[iy] = ix;
            }
            if (rowLastValid[iy] < 0 || ix > rowLastValid[iy])
            {
                rowLastValid[iy] = ix;
            }
            if (columnFirstValid[ix] < 0 || iy < columnFirstValid[ix])
            {
                columnFirstValid[ix] = iy;
            }
            if (columnLastValid[ix] < 0 || iy > columnLastValid[ix])
            {
                columnLastValid[ix] = iy;
            }
        }
    }

    std::vector<int> edgeTileIndices;
    edgeTileIndices.reserve(groundTiles.size());
    constexpr int neighborOffsets[4][2] = {
        { -1, 0 },
        { 1, 0 },
        { 0, -1 },
        { 0, 1 },
    };
    for (int i = 0; i < static_cast<int>(groundTiles.size()); ++i)
    {
        const auto& tile = groundTiles[i];
        bool isEdge = false;
        for (const auto& offset : neighborOffsets)
        {
            const int nx = tile.ix + offset[0];
            const int ny = tile.iy + offset[1];
            if (nx < 0 || nx >= coordCount_ || ny < 0 || ny >= coordCount_
                || groundTileIndices[tileIndex(nx, ny)] < 0)
            {
                isEdge = true;
                break;
            }
        }
        if (isEdge)
        {
            edgeTileIndices.push_back(i);
        }
    }
    if (edgeTileIndices.empty())
    {
        for (int i = 0; i < static_cast<int>(groundTiles.size()); ++i)
        {
            edgeTileIndices.push_back(i);
        }
    }

    auto originalTileIndex = [&](int ix, int iy)
    {
        if (ix < 0 || ix >= coordCount_ || iy < 0 || iy >= coordCount_)
        {
            return -1;
        }
        return groundTileIndices[tileIndex(ix, iy)];
    };
    auto nearestEdgeTileIndex = [&](int ix, int iy)
    {
        int bestIndex = -1;
        int bestDistance = std::numeric_limits<int>::max();
        for (int sourceIndex : edgeTileIndices)
        {
            const auto& tile = groundTiles[sourceIndex];
            const int dx = tile.ix - ix;
            const int dy = tile.iy - iy;
            const int distance = dx * dx + dy * dy;
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestIndex = sourceIndex;
            }
        }
        return bestIndex;
    };
    auto extensionSourceIndex = [&](int ix, int iy)
    {
        const int originalIndex = originalTileIndex(ix, iy);
        if (originalIndex >= 0)
        {
            return originalIndex;
        }

        int sx = std::clamp(ix, 0, coordCount_ - 1);
        int sy = std::clamp(iy, 0, coordCount_ - 1);
        if (iy < 0 && columnFirstValid[sx] >= 0)
        {
            sy = columnFirstValid[sx];
        }
        else if (iy >= coordCount_ && columnLastValid[sx] >= 0)
        {
            sy = columnLastValid[sx];
        }
        if (ix < 0 && rowFirstValid[sy] >= 0)
        {
            sx = rowFirstValid[sy];
        }
        else if (ix >= coordCount_ && rowLastValid[sy] >= 0)
        {
            sx = rowLastValid[sy];
        }

        const int clampedIndex = originalTileIndex(sx, sy);
        if (clampedIndex >= 0)
        {
            return clampedIndex;
        }
        return nearestEdgeTileIndex(ix, iy);
    };

    const int extensionMargin = std::max(0, baseEarthOffsetX / (Scene::TILE_W * 2));
    std::vector<GroundDrawTile> groundDrawTiles;
    groundDrawTiles.reserve((coordCount_ + extensionMargin * 2) * (coordCount_ + extensionMargin * 2));
    for (int ix = -extensionMargin; ix < coordCount_ + extensionMargin; ++ix)
    {
        for (int iy = -extensionMargin; iy < coordCount_ + extensionMargin; ++iy)
        {
            const int sourceIndex = extensionSourceIndex(ix, iy);
            if (sourceIndex >= 0)
            {
                groundDrawTiles.push_back({ sourceIndex, ix, iy });
            }
        }
    }

    auto renderGroundTile = [&](const GroundDrawTile& drawTile)
    {
        const auto& sourceTile = groundTiles[drawTile.sourceIndex];
        auto tex = sourceTile.texture;
        if (!tex)
        {
            return;
        }
        tex->load();
        auto texture = tex->getTexture();
        if (!texture)
        {
            return;
        }
        Engine::setColor(texture, { 255, 255, 255, 255 });
        const auto basePosition = positionOnWholeEarth(drawTile.ix, drawTile.iy, renderCenterX, renderCenterY);
        const int drawX = basePosition.x + baseEarthOffsetX;
        const int drawY = basePosition.y + baseEarthOffsetY;
        engine->renderTexture(texture, drawX, drawY, tex->w, tex->h);
    };
    for (const auto& tile : groundDrawTiles)
    {
        renderGroundTile(tile);
    }

    auto battleEarthGroup = textureManager->getTextureGroup("battle-earth");
    if (battleEarthGroup->getTextureCount() > 0)
    {
        auto tex = textureManager->getTexture("battle-earth", battlefieldId_);
        if (tex)
        {
            tex->load();
            auto texture = tex->getTexture();
            if (texture)
            {
                int textureW = tex->w;
                int textureH = tex->h;
                engine->getTextureSize(texture, textureW, textureH);
                Engine::setColor(texture, { 255, 255, 255, 255 });
                if (textureW > 0 && textureH > 0)
                {
                    const int centerX = baseEarthOffsetX - tex->dx;
                    const int centerY = baseEarthOffsetY - tex->dy;
                    const int rightX = centerX + baseEarthW;
                    const int bottomY = centerY + baseEarthH;
                    const int leftW = std::max(0, centerX);
                    const int topH = std::max(0, centerY);
                    const int rightW = std::max(0, baseEarthTextureW - rightX);
                    const int bottomH = std::max(0, baseEarthTextureH - bottomY);
                    const int stripW = std::clamp(textureW / PAPER_GROUND_EDGE_STRETCH_SOURCE_DIVISOR, 1, textureW);
                    const int stripH = std::clamp(textureH / PAPER_GROUND_EDGE_STRETCH_SOURCE_DIVISOR, 1, textureH);
                    auto renderTextureRect = [&](Rect source, Rect destination)
                    {
                        if (source.w > 0 && source.h > 0 && destination.w > 0 && destination.h > 0)
                        {
                            engine->renderTexture(texture, &source, &destination);
                        }
                    };
                    renderTextureRect({ 0, 0, textureW, textureH }, { centerX, centerY, baseEarthW, baseEarthH });
                    renderTextureRect({ 0, 0, stripW, textureH }, { 0, centerY, leftW, baseEarthH });
                    renderTextureRect({ textureW - stripW, 0, stripW, textureH }, { rightX, centerY, rightW, baseEarthH });
                    renderTextureRect({ 0, 0, textureW, stripH }, { centerX, 0, baseEarthW, topH });
                    renderTextureRect({ 0, textureH - stripH, textureW, stripH }, { centerX, bottomY, baseEarthW, bottomH });
                    renderTextureRect({ 0, 0, stripW, stripH }, { 0, 0, leftW, topH });
                    renderTextureRect({ textureW - stripW, 0, stripW, stripH }, { rightX, 0, rightW, topH });
                    renderTextureRect({ 0, textureH - stripH, stripW, stripH }, { 0, bottomY, leftW, bottomH });
                    renderTextureRect({ textureW - stripW, textureH - stripH, stripW, stripH }, { rightX, bottomY, rightW, bottomH });
                }
            }
        }
    }

    engine->setRenderTarget(earthTexture);
    engine->fillColor({ 0, 0, 0, 0 }, 0, 0, paperEarthTextureW, paperEarthTextureH, BLENDMODE_NONE);
    Engine::setColor(earthBaseTexture, { 255, 255, 255, 255 });
    Rect baseSourceRect = { 0, 0, baseEarthTextureW, baseEarthTextureH };
    Rect paperDestinationRect = { 0, 0, paperEarthTextureW, paperEarthTextureH };
    engine->renderTexture(earthBaseTexture, &baseSourceRect, &paperDestinationRect);

    engine->resetRenderTarget();
}
