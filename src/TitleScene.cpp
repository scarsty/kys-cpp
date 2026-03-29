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
#include "Video.h"
#include "Weather.h"
#include "ChessModHook.h"
#include "filefunc.h"
#include <cstdlib>
#include <vector>

#ifdef __EMSCRIPTEN__
extern "C" void notify_fonts_loaded();
#endif

namespace
{

constexpr int kExternalSaveDialogPollMs = 16;

std::shared_ptr<DrawableOnCall> makeModalBackdrop()
{
    return std::make_shared<DrawableOnCall>([](DrawableOnCall*) {
        Engine::getInstance()->fillColor({0, 0, 0, 255}, 0, 0, -1, -1);
    });
}

std::shared_ptr<TextBox> makeModalLabel(const std::string& text, int x, int y)
{
    auto label = std::make_shared<TextBox>();
    label->setText(text);
    label->setFontSize(32);
    label->setHaveBox(false);
    label->setTextColor({255, 220, 80});
    label->setPosition(x, y);
    return label;
}

int runModalNode(const std::shared_ptr<RunNode>& node, const std::shared_ptr<TextBox>& label = nullptr)
{
    auto bg = makeModalBackdrop();
    RunNode::addIntoDrawTop(bg);
    if (label)
    {
        RunNode::addIntoDrawTop(label);
    }
    int result = node->run();
    if (label)
    {
        RunNode::removeFromDraw(label);
    }
    RunNode::removeFromDraw(bg);
    return result;
}

void showMessageBox(const std::string& title, const std::string& message, SDL_MessageBoxFlags flags = SDL_MESSAGEBOX_INFORMATION)
{
#ifdef __EMSCRIPTEN__
    auto prompt = std::make_shared<MenuText>(std::vector<std::string>{"確 定"});
    prompt->setFontSize(32);
    prompt->setPosition(560, 420);
    prompt->arrange(0, 0, 0, 0);

    auto titleLabel = makeModalLabel(title, 430, 220);
    auto messageLabel = std::make_shared<TextBox>();
    messageLabel->setText(message);
    messageLabel->setFontSize(28);
    messageLabel->setHaveBox(false);
    messageLabel->setTextColor({220, 220, 220});
    messageLabel->setPosition(290, 300);

    auto bg = makeModalBackdrop();
    RunNode::addIntoDrawTop(bg);
    RunNode::addIntoDrawTop(titleLabel);
    RunNode::addIntoDrawTop(messageLabel);
    prompt->run();
    RunNode::removeFromDraw(messageLabel);
    RunNode::removeFromDraw(titleLabel);
    RunNode::removeFromDraw(bg);
#else
    (void)flags;
    SDL_ShowSimpleMessageBox(flags, title.c_str(), message.c_str(), Engine::getInstance()->getWindow());
#endif
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

#ifdef __EMSCRIPTEN__
void openWebExternalSaveDialog(const std::string& title, const std::string& initialText, bool importMode)
{
    EM_ASM({
        if (Module.kysOpenExternalSaveDialog) {
            Module.kysOpenExternalSaveDialog(UTF8ToString($0), UTF8ToString($1), !!$2);
        }
    }, title.c_str(), initialText.c_str(), importMode ? 1 : 0);
}

int pollWebExternalSaveDialog()
{
    return EM_ASM_INT({
        return Module.kysPollExternalSaveDialog ? Module.kysPollExternalSaveDialog() : 2;
    });
}

std::string takeWebExternalSaveDialogText()
{
    const int length = EM_ASM_INT({
        return Module.kysGetExternalSaveDialogTextLength ? Module.kysGetExternalSaveDialogTextLength() : 0;
    });

    std::vector<char> buffer(static_cast<size_t>(length) + 1, '\0');
    EM_ASM({
        if (Module.kysWriteExternalSaveDialogTextToBuffer) {
            Module.kysWriteExternalSaveDialogTextToBuffer($0, $1);
        }
    }, buffer.data(), static_cast<int>(buffer.size()));

    return std::string(buffer.data());
}

void closeWebExternalSaveDialog()
{
    EM_ASM({
        if (Module.kysCloseExternalSaveDialog) {
            Module.kysCloseExternalSaveDialog();
        }
    });
}

bool runWebExternalSaveDialog(const std::string& title, bool importMode, std::string& text)
{
    openWebExternalSaveDialog(title, text, importMode);
    while (true)
    {
        const int state = pollWebExternalSaveDialog();
        if (state == 0)
        {
            emscripten_sleep(kExternalSaveDialogPollMs);
            continue;
        }
        if (state == 1)
        {
            text = takeWebExternalSaveDialogText();
            closeWebExternalSaveDialog();
            return true;
        }
        closeWebExternalSaveDialog();
        return false;
    }
}
#endif

}    // namespace

TitleScene::TitleScene()
{
    full_window_ = 1;
    battle_mode_ = GameUtil::getInstance()->getInt("game", "battle_mode");
    // Text-only menu: 4 items, each 4 CJK chars at size 36 ~= 144px wide.
    // Spacing 204px (144 + 60 gap), total span 756px, centered: x = (1280-756)/2 = 262.
    auto mt = std::make_shared<MenuText>();
    mt->setFontSize(36);
    mt->setHaveBox(true);
    mt->setStrings({"重新開始", "載入進度", "外部存檔", "離開遊戲"});
    mt->setPosition(262, 500);
    mt->arrange(0, 0, 204, 0);
    for (auto& c : mt->getChilds())
    {
        auto* btn = dynamic_cast<Button*>(c.get());
        if (btn)
        {
            btn->setTextOnly(true);
            btn->setHaveBox(false);
            btn->setTextColor({ 200, 50, 50, 255 });
        }
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
    engine->fillColor({ 0, 0, 0, 255 }, 0, 0, w, h);
    if (hide_title_art_for_external_save_)
    {
        return;
    }

    // Chess piece (king) in ASCII art — each char is 8px wide at size 16
    const char* chess[] = {
        "    +    ",
        "   +++   ",
        "    +    ",
        "  +---+  ",
        " | . . | ",
        " |  .  | ",
        " | . . | ",
        "  +---+  ",
        " /     \\ ",
        "/       \\",
        "+-------+",
        "|       |",
        "+-------+",
        " \\_____/ ",
        "/_______\\",
    };

    int chessSize = 16;
    int chessW = 9 * (chessSize / 2);    // 9 chars wide, ASCII = half size
    int chessX = (w - chessW) / 2;
    int chessY = 120;
    Color gold = { 200, 180, 100, 255 };
    for (int i = 0; i < 15; i++)
    {
        Font::getInstance()->draw(chess[i], chessSize, chessX, chessY + i * chessSize, gold, 255);
    }

    // Title: "金群自走棋" in red, centered
    int titleSize = 48;
    int titleW = 5 * titleSize;    // 5 CJK chars, each = titleSize width
    int titleX = (w - titleW) / 2;
    int titleY = chessY + 15 * chessSize + 30;
    Font::getInstance()->draw("金群自走棋", titleSize, titleX, titleY, { 220, 40, 40, 255 }, 255);

    // Version string below title
    Font::getInstance()->draw(GameUtil::VERSION(), 20, titleX, titleY + titleSize + 16, { 150, 150, 150, 255 }, 255);

#ifdef __EMSCRIPTEN__
    Font::getInstance()->draw("推荐使用 Edge / Firefox / Chrome / Opera 浏览器", 16, 10, 10, { 180, 180, 180, 180 }, 255);
    if (!overlay_dismissed_)
    {
        overlay_dismissed_ = true;
        notify_fonts_loaded();
    }
#endif
}

void TitleScene::dealEvent(EngineEvent& e)
{
    int r = menu_->run();
    if (r == 0)
    {
        Engine::getInstance()->gameControllerRumble(50, 50, 500);
        Save::getInstance()->load(0);
        {
            MainScene::getInstance()->setManPosition(Save::getInstance()->MainMapX, Save::getInstance()->MainMapY);
            MainScene::getInstance()->setTowards(1);
            int s = 0, x = 0, y = 0, ev = -1;
            KysChess::ChessModHook::overrideNewGame(s, x, y, ev);
            MainScene::getInstance()->forceEnterSubScene(s, x, y, ev);
            ScenePreloader::showPromptAndPreload("加載中...", []() {
                ScenePreloader::preloadSubSceneAssets(53);
            });
            MainScene::getInstance()->run();
        }
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
    auto menu = std::make_shared<MenuText>();
    menu->setFontSize(32);
    menu->setStrings({
        getSlotSummary(1),
        getSlotSummary(2),
        getSlotSummary(3),
        getSlotSummary(static_cast<int>(UISave::Slot::Auto))
    });
    menu->setPosition(400, 240);
    menu->arrange(0, 0, 0, 42);

    auto label = makeModalLabel("選擇要處理的存檔", 420, 180);
    const int result = runModalNode(menu, label);
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
    auto menu = std::make_shared<MenuText>();
    menu->setFontSize(32);
    menu->setStrings({"匯出 JSON", "匯入 JSON", "取消"});
    menu->setPosition(470, 290);
    menu->arrange(0, 0, 0, 52);

    auto label = makeModalLabel(std::format("{} 外部存檔", getSlotTitle(slot)), 430, 220);
    const int result = runModalNode(menu, label);
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

#ifdef __EMSCRIPTEN__
    auto title = std::format("{} 匯出 JSON", getSlotTitle(slot));
    runWebExternalSaveDialog(title, false, payload);
#else
    if (!SDL_SetClipboardText(payload.c_str()))
    {
        showMessageBox("外部存檔", std::format("{} 匯出失敗。\n{}", getSlotTitle(slot), SDL_GetError()), SDL_MESSAGEBOX_ERROR);
        return;
    }
    showMessageBox("外部存檔", std::format("{} JSON 已複製到剪貼簿。", getSlotTitle(slot)));
#endif
}

void TitleScene::importExternalSaveSlot(int slot)
{
    std::string payload;
    Save::getInstance()->exportSlotJson(slot, payload);

#ifdef __EMSCRIPTEN__
    auto title = std::format("{} 匯入 JSON", getSlotTitle(slot));
    if (!runWebExternalSaveDialog(title, true, payload))
    {
        return;
    }
#else
    char* clipboardText = SDL_GetClipboardText();
    if (clipboardText)
    {
        payload = clipboardText;
        SDL_free(clipboardText);
    }
    if (payload.empty())
    {
        showMessageBox("外部存檔", "請先把存檔 JSON 複製到剪貼簿，再執行匯入。", SDL_MESSAGEBOX_WARNING);
        return;
    }
#endif

    if (!Save::getInstance()->importSlotJson(slot, payload))
    {
        showMessageBox("外部存檔", std::format("{} 匯入失敗，請確認貼上的內容是完整 JSON。", getSlotTitle(slot)), SDL_MESSAGEBOX_ERROR);
        return;
    }
    menu_load_->refreshEntries();
}

void TitleScene::onEntrance()
{
    Video v(Engine::getInstance()->getWindow());
    v.playVideo(GameUtil::PATH() + "movie/1.mp4");
    Audio::getInstance()->playMusic(16);

#ifdef __EMSCRIPTEN__
    bool hasAutoSave = Save::getInstance()->checkSaveFileExist(static_cast<int>(UISave::Slot::Auto));
    if (hasAutoSave)
    {
        auto prompt = std::make_shared<MenuText>(std::vector<std::string>{"  是  ", "  否  "});
        prompt->setFontSize(32);
        prompt->arrange(0, 0, 200, 0);
        prompt->setPosition(440, 400);
        auto label = std::make_shared<TextBox>();
        label->setText("檢測到自動存檔，是否繼續？");
        label->setFontSize(32);
        label->setHaveBox(false);
        label->setTextColor({255, 220, 80});
        prompt->addChild(label, -200, -60);
        auto bg = std::make_shared<DrawableOnCall>([](DrawableOnCall*) {
            Engine::getInstance()->fillColor({0, 0, 0, 180}, 200, 300, 880, 180);
        });
        RunNode::addIntoDrawTop(bg);
        int r = prompt->run();
        RunNode::removeFromDraw(bg);
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
