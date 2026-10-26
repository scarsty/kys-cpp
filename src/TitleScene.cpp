#include "TitleScene.h"
#include "Audio.h"
#include "BattleScene.h"
#include "Button.h"
#include "Font.h"
#include "GameUtil.h"
#include "INIReader.h"
#include "InputBox.h"
#include "MainScene.h"
#include "Menu.h"
#include "Random.h"
#include "RandomRole.h"
#include "Save.h"
#include "ScenePreloader.h"
#include "SubScene.h"
#include "UISave.h"
#include "DrawableOnCall.h"
#include "ExternalJsonFileTransfer.h"
#include "ChessUiCommon.h"
#include "ChessUiStyle.h"
#include "ChessTalentUi.h"
#include "ChessSelectionScreen.h"
#include "ChessScreenLayout.h"
#include "Video.h"
#include "Weather.h"
#include "ChessBalance.h"
#include "ChessApplicationSessionHost.h"
#include "ChessGameContent.h"
#include "ChessModHook.h"
#include "TextureManager.h"
#include "filefunc.h"
#include "ImGuiLayer.h"
#include "strfunc.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <format>
#include <initializer_list>
#include <numbers>
#include <sstream>
#include <vector>

#ifdef __EMSCRIPTEN__
extern "C" void notify_fonts_loaded();
#endif

namespace
{

void drawTitleEmblem(float centerX)
{
    using namespace KysChess::ChessUiStyle;
    auto* engine = Engine::getInstance();
    constexpr float centerY = 232.0f;
    constexpr float tau = 2.0f * std::numbers::pi_v<float>;
    constexpr double cycleMs = 18000.0;
    const float phase = static_cast<float>(std::fmod(Engine::getTicks(), cycleMs) / cycleMs) * tau;

    // 所有路徑使用邏輯座標，交由引擎統一縮放；不載入圖片或額外字形。
    const auto stroke = [&](FPoint a, FPoint b, Color color, float width = 1.0f) {
        const float length = std::hypot(b.x - a.x, b.y - a.y);
        const float nx = -(b.y - a.y) / length * (width - 1.0f) * 0.5f;
        const float ny = (b.x - a.x) / length * (width - 1.0f) * 0.5f;
        engine->drawLine(color, {centerX + a.x + nx, centerY + a.y + ny},
            {centerX + b.x + nx, centerY + b.y + ny});
        if (width > 1.0f)
            engine->drawLine(color, {centerX + a.x - nx, centerY + a.y - ny},
                {centerX + b.x - nx, centerY + b.y - ny});
    };
    const auto path = [&](std::initializer_list<FPoint> points, Color color, float width = 2.0f) {
        for (auto point = points.begin() + 1; point != points.end(); ++point)
            stroke(*(point - 1), *point, color, width);
    };
    const auto curve = [&](FPoint a, FPoint b, FPoint c, FPoint d, Color color, float width = 2.0f) {
        FPoint previous = a;
        for (int i = 1; i <= 20; ++i)
        {
            const float t = i / 20.0f, u = 1.0f - t;
            const FPoint next{u * u * u * a.x + 3 * u * u * t * b.x + 3 * u * t * t * c.x + t * t * t * d.x,
                u * u * u * a.y + 3 * u * u * t * b.y + 3 * u * t * t * c.y + t * t * t * d.y};
            stroke(previous, next, color, width);
            previous = next;
        }
    };
    const auto arc = [&](float radius, float start, float sweep, Color color, bool tapered) {
        constexpr int segments = 72;
        FPoint previous{radius * std::cos(start), radius * std::sin(start)};
        for (int i = 1; i <= segments; ++i)
        {
            const float t = i / float(segments);
            const float angle = start + sweep * t;
            const FPoint next{radius * std::cos(angle), radius * std::sin(angle)};
            auto shade = color;
            if (tapered) shade.a = static_cast<uint8_t>(color.a * std::sin(t * std::numbers::pi_v<float>));
            stroke(previous, next, shade);
            previous = next;
        }
    };

    // 月輪與緩慢流轉的劍氣；固定輪廓在動畫的每一刻都清晰可辨。
    arc(128, 0, tau, line, false);
    auto aura = gold;
    aura.a = 120;
    arc(122, phase, tau * 0.34f, aura, true);
    arc(136, -phase + tau * 0.5f, tau * 0.24f, aura, true);
    path({{-178, 0}, {-150, 0}}, line, 1);
    path({{150, 0}, {178, 0}}, line, 1);

    // 以劍為冠、束腰為身、雙層棋座的武俠棋子。
    path({{0, -112}, {-9, -88}, {-7, -55}, {7, -55}, {9, -88}, {0, -112}}, gold);
    path({{0, -96}, {0, -61}}, ink, 1);
    path({{-26, -62}, {-20, -54}, {20, -54}, {26, -62}}, gold);
    path({{-5, -53}, {-5, -38}, {5, -38}, {5, -53}}, gold);
    path({{-18, -37}, {18, -37}, {23, -28}, {-23, -28}, {-18, -37}}, gold);
    for (float side : {-1.0f, 1.0f})
    {
        curve({side * 23, -28}, {side * 45, -17}, {side * 28, -2}, {side * 22, 12}, gold);
        curve({side * 22, 12}, {side * 18, 34}, {side * 32, 57}, {side * 45, 72}, gold);
        curve({side * 16, 20}, {side * 15, 40}, {side * 22, 52}, {side * 29, 61}, line, 1);
    }
    path({{-22, 10}, {22, 10}}, gold);
    path({{-22, 17}, {22, 17}}, gold);
    path({{0, -13}, {-6, -4}, {0, 5}, {6, -4}, {0, -13}}, ink, 1);
    path({{-45, 72}, {45, 72}, {49, 81}, {-49, 81}, {-45, 72}}, gold);
    path({{-43, 81}, {-55, 94}, {-55, 102}, {55, 102}, {55, 94}, {43, 81}}, gold);
    path({{-46, 94}, {46, 94}}, gold, 1);

    // 劍穗隨風輕擺，使用連續曲線而非粒子或貼圖。
    const float breeze = std::sin(phase * 4) * 5.0f;
    auto silk = gold;
    silk.a = 165;
    curve({20, -54}, {45, -48}, {34 + breeze, -24}, {51 + breeze, -12}, silk, 1);
    curve({20, -54}, {40, -40}, {29 + breeze, -19}, {46 + breeze, -5}, silk, 1);
}

int fitTextSizeToWidth(const std::string& text, int maxWidth, int maxSize)
{
    const int textUnits = std::max(1, Font::getTextDrawSize(text));
    const int fittedSize = std::max(1, maxWidth * 2 / textUnits);
    return std::clamp(fittedSize, 1, maxSize);
}

int rightAlignTextX(const std::string& text, int textSize, int rightMargin, int uiWidth)
{
    return uiWidth - rightMargin - textSize * Font::getTextDrawSize(text) / 2;
}

std::shared_ptr<DrawableOnCall> makeModalBackdrop()
{
    return std::make_shared<DrawableOnCall>([](DrawableOnCall*) {
        Engine::getInstance()->fillColor({0, 0, 0, 255}, 0, 0, -1, -1);
    });
}

class ChangelogWindowWaitNode : public RunNode
{
public:
    void backRun() override
    {
        if (!Engine::getInstance()->isChangelogWindowOpen())
        {
            exitWithResult(0);
        }
    }
};

ChangelogData loadChangelogData()
{
    ChangelogData data;
    data.title = "更新日誌";

    const std::vector<std::string> candidates = {
        GameUtil::PATH() + "config/changelog.md",
        GameUtil::PATH() + "config/更新日志.md",
        "docs/更新日志.md",
        "../docs/更新日志.md",
    };

    std::string content;
    for (const auto& path : candidates)
    {
        if (!filefunc::fileExist(path))
        {
            continue;
        }
        content = filefunc::readFileToString(path);
        if (!content.empty())
        {
            data.sourcePath = path;
            break;
        }
    }

    if (content.empty())
    {
        data.error = "找不到更新日誌。請確認 docs/更新日志.md 已複製到 game/config/changelog.md。";
        return data;
    }

    if (content.size() >= 3 && static_cast<uint8_t>(content[0]) == 0xEF && static_cast<uint8_t>(content[1]) == 0xBB && static_cast<uint8_t>(content[2]) == 0xBF)
    {
        content.erase(0, 3);
    }

    std::istringstream input(content);
    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        const size_t firstText = line.find_first_not_of(" \t");
        const int leadingSpaces = firstText == std::string::npos ? 0 : static_cast<int>(firstText);
        auto trimmed = strfunc::trim(line);
        if (trimmed.empty())
        {
            data.lines.push_back({});
            continue;
        }

        if (trimmed[0] == '#')
        {
            int hashes = 0;
            while (hashes < static_cast<int>(trimmed.size()) && trimmed[hashes] == '#')
            {
                ++hashes;
            }
            auto heading = strfunc::trim(trimmed.substr(hashes));
            if (heading == "更新日志" || heading == "更新日誌")
            {
                data.title = "更新日誌";
                continue;
            }
            data.lines.push_back(ChangelogLine{std::max(1, std::min(hashes, 2)), -1, heading});
            continue;
        }

        if (trimmed.rfind("- ", 0) == 0)
        {
            data.lines.push_back(ChangelogLine{0, leadingSpaces / 4, strfunc::trim(trimmed.substr(2))});
            continue;
        }

        data.lines.push_back(ChangelogLine{0, -1, trimmed});
    }

    return data;
}

int runModalNode(const std::shared_ptr<RunNode>& node, const std::shared_ptr<TextBox>& label = nullptr,
    const std::vector<std::shared_ptr<RunNode>>& overlays = {})
{
    auto bg = makeModalBackdrop();
    RunNode::addIntoDrawTop(bg);
    if (label)
    {
        RunNode::addIntoDrawTop(label);
    }
    for (const auto& overlay : overlays)
    {
        RunNode::addIntoDrawTop(overlay);
    }
    int result = node->run();
    for (auto it = overlays.rbegin(); it != overlays.rend(); ++it)
    {
        RunNode::removeFromDraw(*it);
    }
    if (label)
    {
        RunNode::removeFromDraw(label);
    }
    RunNode::removeFromDraw(bg);
    return result;
}

struct ScopedTitleArtHide
{
    bool& flag;
    bool previous;

    explicit ScopedTitleArtHide(bool& flag) : flag(flag), previous(flag)
    {
        this->flag = true;
    }

    ~ScopedTitleArtHide()
    {
        flag = previous;
    }
};

void showMessageBox(const std::string& title, const std::string& message, SDL_MessageBoxFlags flags = SDL_MESSAGEBOX_INFORMATION)
{
    (void)flags;
    KysChess::showChessMessage(title + "\n\n" + message);
}

std::string getSlotTitle(int slot)
{
    if (slot == static_cast<int>(UISave::Slot::Auto))
    {
        return "自動檔";
    }
    return std::format("進度{:02}", slot);
}

std::string getSlotSummary(int slot)
{
    auto filename = Save::getInstance()->getFilename(slot, '\0');
    auto timestamp = filefunc::getFileTime(filename);
    if (timestamp.empty())
    {
        timestamp = "-------------------";
    }
    return std::format("{}  {}", getSlotTitle(slot), timestamp);
}

}    // namespace

TitleScene::TitleScene()
{
    full_window_ = 1;
    battle_mode_ = GameUtil::getInstance()->getInt("game", "battle_mode");
    // Text-only menu: 5 items, each 4 CJK chars at size 36 ~= 144px wide.
    auto mt = std::make_shared<MenuText>();
    mt->setFontSize(24);
    mt->setHaveBox(true);
    mt->setStrings({"重新開始", "載入進度", "外部存檔", "更新日誌", "離開遊戲"});
    mt->setPosition(120, 520);
    mt->arrange(0, 0, 212, 0);
    const std::array actionIcons{
        KysChess::ChessUiStyle::Icon::Formation,
        KysChess::ChessUiStyle::Icon::Load,
        KysChess::ChessUiStyle::Icon::Transfer,
        KysChess::ChessUiStyle::Icon::Book,
        KysChess::ChessUiStyle::Icon::Close,
    };
    int actionIndex{};
    for (auto& c : mt->getChilds())
    {
        auto* btn = dynamic_cast<Button*>(c.get());
        if (btn)
        {
            btn->setFlatStyle(192, 56);
            btn->setHaveBox(false);
            btn->setTextColor(KysChess::ChessUiStyle::ink);
            btn->setFlatIcon(actionIcons[actionIndex]);
            btn->setFlatPrimary(actionIndex == 0);
        }
        ++actionIndex;
    }
    menu_ = mt;
    menu_load_ = std::make_shared<UISave>();
    menu_load_->setPosition(500, 500);
    //render_message_ = 1;

    if (battle_mode_ == 2)
    {
        // auto pe1 = std::make_shared<ParticleExample>();
        // pe1->setStyle(ParticleExample::FIRE);
        // addChild(pe1);
        // pe1->setPosition(490, 80);
        // pe1->setSize(20, 20);
    }
    else if (battle_mode_ == 3)
    {
        auto pe1 = std::make_shared<ParticleExample>();
        pe1->setPosition(Engine::getInstance()->getWindowWidth() * 0.15, 0);
        pe1->setStyle(ParticleExample::RAIN);    //先设置位置，再设置样式，有var的问题
        pe1->setTotalParticles(2000);
        pe1->setPosVar({ float(Engine::getInstance()->getWindowWidth() * 0.85), -50 });
        pe1->resetSystem();
        pe1->setEmissionRate(200);
        pe1->setGravity({ 200, 50 });
        addChild(pe1);
    }
    //调试用代码
    //Save::getInstance()->load(5);
    RandomDouble rand;
    int k = rand.rand() * 139;
    k = 1;
    //Event::getInstance()->tryBattle(k, 0);

    //auto  p=std::make_shared<RunNodeFromJson>("../game/Scene.json");
    //addChild(p);
}

TitleScene::~TitleScene()
{
}

void TitleScene::draw()
{
    auto engine = Engine::getInstance();
    int w = engine->getUIWidth();
    int h = engine->getUIHeight();
    engine->fillColor({18, 17, 16, 255}, 0, 0, w, h);
    if (hide_title_art_for_external_save_)
    {
        return;
    }

    drawTitleEmblem(w * 0.5f);

    int titleSize = 48;
    int titleW = 5 * titleSize;    // 5 CJK chars, each = titleSize width
    int titleX = (w - titleW) / 2;
    int titleY = 386;
    Font::getInstance()->draw("金群自走棋", titleSize, titleX, titleY, KysChess::ChessUiStyle::gold, 255);
    engine->fillColor(KysChess::ChessUiStyle::line, 120, 504, w - 240, 1);

    const std::string qqGroupText = "QQ群910723737";
    const int qqGroupSize = fitTextSizeToWidth(qqGroupText, w - 20, 18);
    const int qqGroupX = rightAlignTextX(qqGroupText, qqGroupSize, 10, w);
    Font::getInstance()->draw(qqGroupText, qqGroupSize, qqGroupX, h - 30, { 150, 150, 150, 255 }, 255);

    // Version string below title
    Font::getInstance()->draw(GameUtil::VERSION(), 20, 10, h - 30, { 150, 150, 150, 255 }, 255);

#ifdef __EMSCRIPTEN__
    Font::getInstance()->draw("建議使用 Edge / Firefox / Chrome / Opera 瀏覽器", 16, 10, 10, { 180, 180, 180, 180 }, 255);
#endif
}

void TitleScene::dealEvent(EngineEvent& e)
{
    int r = menu_->run();
    if (r == 0)
    {
        // Difficulty selection before entering the game
        ScopedTitleArtHide hideTitleArt(hide_title_art_for_external_save_);
        auto diffMenu = std::make_shared<KysChess::ChessSelectionScreen>("選擇難度", std::vector<KysChess::ChessSelectionItem>{
            {.title = "簡單", .subtitle = "初入江湖", .image = KysChess::chessSelectionIcon(KysChess::ChessUiStyle::Icon::Book), .accent = KysChess::ChessUiStyle::positive, .summary = "固定棋池，熟悉招募與羈絆"},
            {.title = "標準", .subtitle = "行走江湖", .image = KysChess::chessSelectionIcon(KysChess::ChessUiStyle::Icon::Formation), .summary = "透過禁棋調整棋池，打造自己的陣容"},
            {.title = "困難", .subtitle = "問鼎武林", .image = KysChess::chessSelectionIcon(KysChess::ChessUiStyle::Icon::Mountain), .accent = KysChess::ChessUiStyle::negative, .summary = "更長的征途，更緊湊的金幣預算"},
        });
        diffMenu->setActionLabel("下一步");
        diffMenu->setShortChoiceMode();
        KysChess::Difficulty difficulty{};
        KysChess::ChessTalentId talent{};
        for (;;)
        {
            const int diff = runModalNode(diffMenu);
            if (diff < 0) return;
            difficulty = static_cast<KysChess::Difficulty>(diff);
            std::shared_ptr<const KysChess::ChessGameContent> content;
            ScenePreloader::showPromptAndPreload("正在載入棋手天賦…", [&] {
                content = KysChess::ChessApplicationSessionHost::instance().contentFor(difficulty);
                for (const auto id : content->balance().availableTalents)
                    TextureManager::getInstance()->getTexture("chess-talents", static_cast<int>(id))->load();
            });
            const auto& balance = content->balance();
            talent = balance.defaultTalent;
            if (balance.availableTalents.size() == 1) break;
            std::vector<KysChess::ChessSelectionItem> talents;
            for (const auto id : balance.availableTalents)
            {
                const auto presentation = KysChess::buildChessTalentPresentation(balance, id);
                KysChess::ChessSelectionItem card{
                    .title = KysChess::chessTalentName(id),
                    .image = {"chess-talents", static_cast<int>(id)},
                    .talent = presentation,
                };
                talents.push_back(std::move(card));
            }
            auto talentMenu = std::make_shared<KysChess::ChessSelectionScreen>("選擇棋手天賦", std::move(talents));
            talentMenu->setActionLabel("開始棋局");
            talentMenu->setTalentMode();
            talentMenu->setSelectedItem(static_cast<int>(std::ranges::find(balance.availableTalents, talent) - balance.availableTalents.begin()));
            const int selected = runModalNode(talentMenu);
            if (selected < 0) continue;
            talent = balance.availableTalents[selected];
            break;
        }

        Engine::getInstance()->gameControllerRumble(50, 50, 500);
        bool prepared{};
        ScenePreloader::showPromptAndPreload("正在建立棋局與載入場景…", [&] {
            prepared = Save::getInstance()->prepareChessMode();
            if (!prepared) return;
            MainScene::getInstance()->setManPosition(Save::getInstance()->MainMapX, Save::getInstance()->MainMapY);
            MainScene::getInstance()->setTowards(1);
            int s{}, x{}, y{}, ev = -1;
            KysChess::ChessModHook::overrideNewGame(s, x, y, ev, difficulty, talent);
            MainScene::getInstance()->forceEnterSubScene(s, x, y, ev);
            ScenePreloader::preloadSubSceneAssets(53);
        });
        if (!prepared)
        {
            showMessageBox("錯誤", "初始化棋局資料失敗");
            return;
        }
        MainScene::getInstance()->run();
    }
    if (r == 1)
    {
        if (runModalNode(menu_load_) >= 0)
        {
            //Save::getInstance()->getRole(0)->MagicLevel[0] = 900;    //测试用
            //Script::getInstance()->runScript(GameUtil::PATH()+"script/0.lua");
            MainScene::getInstance()->run();
        }
    }
    if (r == 2)
    {
        runExternalSaveFlow();
    }
    if (r == 3)
    {
        Engine::getInstance()->showChangelogWindow(loadChangelogData());
        auto waitNode = std::make_shared<ChangelogWindowWaitNode>();
        waitNode->run();
    }
    if (r == 4)
    {
        setExit(true);
        exit(0);    //强制退出，否则在Android下可能退不完全
    }
}

void TitleScene::runExternalSaveFlow()
{
    hide_title_art_for_external_save_ = true;
    const int slot = promptExternalSaveSlot();
    if (slot < 0)
    {
        hide_title_art_for_external_save_ = false;
        return;
    }

    const int action = promptExternalSaveAction(slot);
    if (action == 0)
    {
        exportExternalSaveSlot(slot);
    }
    else if (action == 1)
    {
        importExternalSaveSlot(slot);
    }
    hide_title_art_for_external_save_ = false;
}

int TitleScene::promptExternalSaveSlot()
{
    const int result = KysChess::runChessChoiceMenu("選擇要處理的存檔", {
        getSlotSummary(1),
        getSlotSummary(2),
        getSlotSummary(3),
        getSlotSummary(static_cast<int>(UISave::Slot::Auto))
    });
    switch (result)
    {
    case 0:
        return 1;
    case 1:
        return 2;
    case 2:
        return 3;
    case 3:
        return static_cast<int>(UISave::Slot::Auto);
    default:
        return -1;
    }
}

int TitleScene::promptExternalSaveAction(int slot)
{
    const int result = KysChess::runChessChoiceMenu(std::format("{} 外部存檔", getSlotTitle(slot)),
        {"匯出完整存檔 JSON", "匯入存檔／對戰 JSON", "取消"});
    if (result == 2)
    {
        return -1;
    }
    return result;
}

void TitleScene::exportExternalSaveSlot(int slot)
{
    std::string payload;
    if (!Save::getInstance()->exportSlotJson(slot, payload))
    {
        showMessageBox("外部存檔", std::format("{} 目前沒有可匯出的 JSON 存檔。", getSlotTitle(slot)), SDL_MESSAGEBOX_WARNING);
        return;
    }

    const auto result = ExternalJsonFileTransfer::exportJson(
        std::format("{} 匯出 JSON", getSlotTitle(slot)),
        std::format("kys-save-{}.json", slot),
        payload);
    if (result.status == ExternalJsonTransferStatus::Error)
    {
        showMessageBox(
            "外部存檔",
            std::format("{} 匯出失敗。\n{}", getSlotTitle(slot), result.error),
            SDL_MESSAGEBOX_ERROR);
    }
}

void TitleScene::importExternalSaveSlot(int slot)
{
    auto transfer = ExternalJsonFileTransfer::importJson(
        std::format("{} 匯入 JSON", getSlotTitle(slot)));
    if (transfer.status == ExternalJsonTransferStatus::Cancelled)
    {
        return;
    }
    if (transfer.status == ExternalJsonTransferStatus::Error)
    {
        showMessageBox(
            "外部存檔",
            std::format("{} 匯入失敗。\n{}", getSlotTitle(slot), transfer.error),
            SDL_MESSAGEBOX_ERROR);
        return;
    }

    if (!Save::getInstance()->importSlotJson(slot, transfer.text))
    {
        showMessageBox(
            "外部存檔",
            std::format(
                "{} 匯入失敗，請確認內容是完整存檔或離線對戰 JSON。",
                getSlotTitle(slot)),
            SDL_MESSAGEBOX_ERROR);
        return;
    }
    menu_load_->refreshEntries();
}

void TitleScene::onEntrance()
{
#ifdef __EMSCRIPTEN__
    // 字型已可用，讓不依賴圖片的載入介面接手開機遮罩。
    if (!overlay_dismissed_)
    {
        overlay_dismissed_ = true;
        notify_fonts_loaded();
    }
#endif
    ScenePreloader::LoadQueue uiLoads;
    uiLoads.addTextureGroup("操作圖示", "chess-preparation");
    uiLoads.addTextureGroup("羈絆圖示", "chess-synergies");
    uiLoads.addTextureGroup("狀態圖示", "chess-status");
    ScenePreloader::showPromptAndPreload("正在載入介面…", std::move(uiLoads));

    Video v(Engine::getInstance()->getWindow());
    v.playVideo(GameUtil::PATH() + "movie/1.mp4");
    Audio::getInstance()->playMusic(16);

#ifdef __EMSCRIPTEN__
    bool hasAutoSave = Save::getInstance()->checkSaveFileExist(static_cast<int>(UISave::Slot::Auto));
    if (hasAutoSave)
    {
        const int r = KysChess::runChessChoiceMenu("檢測到自動存檔，是否繼續？",
            {"繼續棋局", "返回開頭"}, KysChess::ChessChoiceStyle::Confirmation);
        if (r == 0)
        {
            if (UISave::loadAuto())
            {
                MainScene::getInstance()->run();
                return;
            }
        }
    }
#endif
}
