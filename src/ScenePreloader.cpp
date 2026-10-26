#include "ScenePreloader.h"

#include "BattleMap.h"
#include "ChessUiStyle.h"
#include "Engine.h"
#include "Font.h"
#include "RunNode.h"
#include "Save.h"
#include "TextureManager.h"

#include <chrono>
#include <format>
#include <optional>
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
    DeferredPreloadPrompt(std::string message, ScenePreloader::LoadQueue loads)
        : message_(std::move(message)), loads_(std::move(loads))
    {
        // 固定已呈現的背景，避免底層畫面在提示出現前觸發另一筆延遲載入。
        full_window_ = 1;
        auto* engine = Engine::getInstance();
        auto* target = engine->getRenderTarget();
        int width{}, height{};
        engine->getTextureSize(engine->getMainTexture(), width, height);
        background_ = std::format("scene-preload-{}", static_cast<void*>(this));
        engine->createRenderedTexture(background_, width, height);
        engine->setRenderTarget(background_);
        engine->renderTexture(engine->getMainTexture(), 0, 0, width, height);
        engine->setRenderTarget(target);
    }

    ~DeferredPreloadPrompt() override { Engine::getInstance()->destroyTexture(background_); }

    void draw() override
    {
        using namespace KysChess::ChessUiStyle;
        auto* engine = Engine::getInstance();
        engine->renderTexture(engine->getTexture(background_), 0, 0, engine->getUIWidth(), engine->getUIHeight());
        engine->fillColor(canvasScrim, 0, 0, engine->getUIWidth(), engine->getUIHeight());
        panel(360, 264, 560, 192);
        // 提示本身只用文字與幾何圖形，不依賴尚未下載的圖示。
        label(message_, Type::pageTitle, 384, 288, context, 512);
        label(loads_.complete() ? "資源已就緒" : loads_.currentLabel(), Type::body, 384, 330, ink, 440);
        label(std::format("{}%", static_cast<int>(loads_.fraction() * 100)), Type::metadata, 844, 332, context, 60);
        engine->fillColor(activeFill, 384, 366, 512, 4);
        engine->fillColor(context, 384, 366, static_cast<int>(512 * loads_.fraction()), 4);
        label("請稍候，完成後將自動繼續", Type::body, 384, 398, muted, 512);
        promptRendered_ = true;
    }

    PointerResult onPointerEvent(const PointerEvent&) override { return PointerResult::Handled; }

    void dealEvent(EngineEvent& event) override
    {
        event = {};
        if (!promptRendered_) return;
        if (loads_.complete()) { setExit(true); return; }
        // 每批之前都要先呈現進度；一個事件迴圈可能處理多筆輸入。
        promptRendered_ = false;

#ifdef __EMSCRIPTEN__
        // 即使上一幀繪製超時，也先讓瀏覽器呈現提示再開始同步載入。
        emscripten_sleep(0);
#endif
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6);
        do
        {
            loads_.advance();
        } while (!loads_.complete() && std::chrono::steady_clock::now() < deadline);
#ifdef __EMSCRIPTEN__
        // 先接收同步載入期間累積的瀏覽器輸入，再一併丟棄。
        emscripten_sleep(0);
#endif
        auto& input = PointerInput::instance();
        input.pumpSdlEvents();
        input.discardPendingUserInput();
        invalidatePointerOwnership();
    }

private:
    std::string message_;
    ScenePreloader::LoadQueue loads_;
    std::string background_;
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

TextureLoadBatch::TextureLoadBatch(std::vector<TextureWarpper*> textures)
    : pending_(std::move(textures)), total_(pending_.size())
{
    std::erase_if(pending_, [](const auto* texture) { return !texture || texture->loaded; });
}

LoadProgress TextureLoadBatch::progress() const { return {total_ - pending_.size() + index_, total_}; }

LoadProgress TextureLoadBatch::advance()
{
    assert(index_ < pending_.size());
    pending_[index_++]->load();
    return progress();
}

void LoadQueue::addTexture(const std::string& label, const std::string& group, int id)
{
    if (id < 0) return;
    const auto* cached = TextureManager::getInstance()->findTexture(group, id);
    add(label, [group, id] {
        preloadTexture(group, id);
        return LoadProgress{1, 1};
    }, {cached && cached->loaded ? 1u : 0u, 1});
}

void LoadQueue::addTextureGroup(const std::string& label, const std::string& group)
{
    std::optional<TextureLoadBatch> batch;
    if (const auto* cached = TextureManager::getInstance()->findTextureGroup(group)) batch.emplace(cached->group_);
    const auto progress = batch ? batch->progress() : LoadProgress{};
    add(label, [group, batch = std::move(batch)]() mutable {
        if (!batch)
        {
            batch.emplace(TextureManager::getInstance()->getTextureGroup(group)->group_);
            return batch->progress();
        }
        return batch->advance();
    }, progress);
}

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

void showPromptAndPreload(const std::string& message, LoadQueue loads)
{
    if (loads.complete()) return;
    auto prompt = std::make_shared<DeferredPreloadPrompt>(message, std::move(loads));
    prompt->run();
}

void showPromptAndPreload(const std::string& message, const std::function<void()>& preload)
{
    LoadQueue loads;
    loads.add("正在準備資料與場景", [preload] {
        preload();
        return LoadProgress{1, 1};
    });
    showPromptAndPreload(message, std::move(loads));
}

}
