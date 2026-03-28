#include "ScenePreloader.h"

#include "BattleMap.h"
#include "Engine.h"
#include "Font.h"
#include "RunNode.h"
#include "Save.h"
#include "TextureManager.h"

#include <unordered_set>

namespace
{

void preloadTexture(const std::string& group, int id)
{
    if (id < 0)
    {
        return;
    }

    auto* texture = TextureManager::getInstance()->getTexture(group, id);
    if (texture)
    {
        texture->load();
    }
}

class DeferredPreloadPrompt : public RunNode
{
public:
    DeferredPreloadPrompt(std::string message, std::function<void()> preload)
        : message_(std::move(message)), preload_(std::move(preload))
    {
    }

    void draw() override
    {
        auto* engine = Engine::getInstance();
        engine->fillColor({0, 0, 0, 220}, 0, 0, engine->getUIWidth(), engine->getUIHeight());
        Font::getInstance()->drawWithBoxCentered(message_, 32, engine->getUIHeight() / 2 - 16);
        promptRendered_ = true;
    }

    void dealEvent(EngineEvent&) override
    {
        if (!promptRendered_ || preloadDone_)
        {
            return;
        }

        if (preload_)
        {
            preload_();
        }
        preloadDone_ = true;
        setExit(true);
    }

private:
    std::string message_;
    std::function<void()> preload_;
    bool promptRendered_ = false;
    bool preloadDone_ = false;
};

void collectBattleLayerTextures(MapSquareInt& layer, std::unordered_set<int>& smapIds)
{
    for (int y = 0; y < layer.size(); ++y)
    {
        for (int x = 0; x < layer.size(); ++x)
        {
            int num = layer.data(x, y) / 2;
            if (num > 0)
            {
                smapIds.insert(num);
            }
        }
    }
}

}

namespace ScenePreloader
{

void preloadSubSceneAssets(int submapId)
{
    auto* submap = Save::getInstance()->getSubMapInfo(submapId);
    if (!submap)
    {
        return;
    }

    auto* textureManager = TextureManager::getInstance();
    if (textureManager->getTextureGroupCount("smap") <= 0)
    {
        return;
    }

    std::unordered_set<int> smapIds;
    smapIds.reserve(SUBMAP_COORD_COUNT * 3);
    smapIds.insert(1);
    for (int pic = 2501; pic < 2501 + 28; ++pic)
    {
        smapIds.insert(pic);
    }

    for (int y = 0; y < SUBMAP_COORD_COUNT; ++y)
    {
        for (int x = 0; x < SUBMAP_COORD_COUNT; ++x)
        {
            int earth = submap->Earth(x, y) / 2;
            if (earth > 0)
            {
                smapIds.insert(earth);
            }

            int building = submap->Building(x, y) / 2;
            if (building > 0)
            {
                smapIds.insert(building);
            }

            int decoration = submap->Decoration(x, y) / 2;
            if (decoration > 0)
            {
                smapIds.insert(decoration);
            }

            auto* event = submap->Event(x, y);
            if (event)
            {
                int eventPic = event->CurrentPic / 2;
                if (eventPic > 0)
                {
                    smapIds.insert(eventPic);
                }
            }
        }
    }

    for (int id : smapIds)
    {
        preloadTexture("smap", id);
    }

    if (textureManager->getTextureGroup("smap-earth")->getTextureCount() > 0)
    {
        preloadTexture("smap-earth", submapId);
    }
}

void preloadBattleAssets(int battleId)
{
    auto* info = BattleMap::getInstance()->getBattleInfo(battleId);
    if (!info)
    {
        return;
    }

    auto* textureManager = TextureManager::getInstance();
    if (textureManager->getTextureGroupCount("smap") <= 0)
    {
        return;
    }

    MapSquareInt earth(BATTLEMAP_COORD_COUNT);
    MapSquareInt building(BATTLEMAP_COORD_COUNT);
    BattleMap::getInstance()->copyLayerData(info->BattleFieldID, 0, &earth);
    BattleMap::getInstance()->copyLayerData(info->BattleFieldID, 1, &building);

    std::unordered_set<int> smapIds;
    smapIds.reserve(BATTLEMAP_COORD_COUNT * 2);
    collectBattleLayerTextures(earth, smapIds);
    collectBattleLayerTextures(building, smapIds);

    for (int id : smapIds)
    {
        preloadTexture("smap", id);
    }

    if (textureManager->getTextureGroup("battle-earth")->getTextureCount() > 0)
    {
        preloadTexture("battle-earth", info->BattleFieldID);
    }
}

void showPromptAndPreload(const std::string& message, const std::function<void()>& preload)
{
    auto prompt = std::make_shared<DeferredPreloadPrompt>(message, preload);
    prompt->run();
}

}
