#include "ScenePreloader.h"

#include "BattleMap.h"
#include "ChessUiStyle.h"
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
        using namespace KysChess::ChessUiStyle;
        auto* engine = Engine::getInstance();
        engine->fillColor(canvasScrim, 0, 0, engine->getUIWidth(), engine->getUIHeight());
        panel(360, 288, 560, 144);
        asset("chess-preparation", iconId(Icon::Load), {388, 318, 32, 32}, false, gold);
        label(message_, 26, 440, 318, ink, 448);
        label("請稍候，完成後將自動繼續", 18, 440, 374, muted, 448);
        promptRendered_ = true;
    }

    PointerResult onPointerEvent(const PointerEvent&) override { return PointerResult::Handled; }

    void dealEvent(EngineEvent&) override
    {
        if (!promptRendered_) return;

#ifdef __EMSCRIPTEN__
        // 即使上一幀繪製超時，也先讓瀏覽器呈現提示再開始同步載入。
        emscripten_sleep(0);
#endif
        preload_();
#ifdef __EMSCRIPTEN__
        // 先接收同步載入期間累積的瀏覽器輸入，再一併丟棄。
        emscripten_sleep(0);
#endif
        auto& input = PointerInput::instance();
        input.pumpSdlEvents();
        input.discardPendingUserInput();
        invalidatePointerOwnership();
        setExit(true);
    }

private:
    std::string message_;
    std::function<void()> preload_;
    bool promptRendered_{};
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

std::vector<std::pair<std::string, int>> battlefieldAssets(int battlefieldId)
{
    std::vector<std::pair<std::string, int>> assets;
    if (battlefieldId < 0)
    {
        return assets;
    }

    auto* textureManager = TextureManager::getInstance();
    if (textureManager->getTextureGroupCount("smap") <= 0)
    {
        return assets;
    }

    MapSquareInt earth(BATTLEMAP_COORD_COUNT);
    MapSquareInt building(BATTLEMAP_COORD_COUNT);
    BattleMap::getInstance()->copyLayerData(battlefieldId, 0, &earth);
    BattleMap::getInstance()->copyLayerData(battlefieldId, 1, &building);

    std::unordered_set<int> smapIds;
    smapIds.reserve(BATTLEMAP_COORD_COUNT * 2);
    collectBattleLayerTextures(earth, smapIds);
    collectBattleLayerTextures(building, smapIds);

    for (int id : smapIds)
    {
        assets.emplace_back("smap", id);
    }

    if (textureManager->getTextureGroup("battle-earth")->getTextureCount() > 0)
    {
        assets.emplace_back("battle-earth", battlefieldId);
    }
    return assets;
}

void showPromptAndPreload(const std::string& message, const std::function<void()>& preload)
{
    auto prompt = std::make_shared<DeferredPreloadPrompt>(message, preload);
    prompt->run();
}

}
