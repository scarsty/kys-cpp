#include "UIConfig.h"

#include "Audio.h"
#include "Engine.h"
#include "Font.h"
#include "GameUtil.h"
#include "TextureManager.h"

#include <algorithm>

int UIConfig::clampIndex(int value, int count)
{
    if (count <= 0)
    {
        return 0;
    }
    return (std::max)(0, (std::min)(value, count - 1));
}

std::vector<std::string> UIConfig::createVolumeOptions()
{
    std::vector<std::string> options;
    for (int value = 0; value <= 100; value += 10)
    {
        options.push_back(std::to_string(value));
    }
    return options;
}

UIConfig::UIConfig()
{
    auto volume_options = createVolumeOptions();
    addOption("音樂音量", "music", "volume", volume_options);
    addOption("戰鬥音效", "music", "volumewav", volume_options);
    addOption("戰鬥模式", "game", "battle_mode", { "回合制", "半即時", "黑帝斯", "隻狼" });
    addOption("遠征隊規則", "game", "expedition33", { "關閉", "開啟" });
    addOption("戰鬥畫法", "game", "battle_presentation", { "經典", "紙片" });
    addOption("格擋輔助", "game", "easy_block", { "關閉", "開啟" });
    addOption("直接勝利", "game", "battle_debug_win", { "關閉", "開啟" });
    addOption("文字顯示", "game", "simplified_chinese", { "繁體", "簡體" });
    addOption("手柄震動", "game", "controller_rumble", { "關閉", "開啟" });
    addOption("全屏", "game", "fullscreen", { "關閉", "開啟" });

    const int button_y = int(items_.size()) * ROW_GAP + BUTTON_TOP_GAP;
    button_ok_ = std::make_shared<Button>();
    button_ok_->setFontSize(24);
    button_ok_->setText("確認");
    addChild(button_ok_, 100, button_y);

    button_cancel_ = std::make_shared<Button>();
    button_cancel_->setFontSize(24);
    button_cancel_->setText("取消");
    addChild(button_cancel_, 200, button_y);

    w_ = ROW_WIDTH;
    h_ = button_y + 30;

    loadConfig();
    refreshTexts();
}

UIConfig::~UIConfig()
{
}

void UIConfig::addOption(const std::string& label, const std::string& section, const std::string& key, const std::vector<std::string>& options)
{
    OptionItem item;
    item.label = label;
    item.section = section;
    item.key = key;
    item.options = options;

    item.row = std::make_shared<Button>();
    item.row->setFontSize(24);
    item.row->setText(label);
    item.row->setHaveBox(false);
    item.row->setTextColor({ 48, 32, 16, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 });
    item.row->setSize(ROW_WIDTH, ROW_HEIGHT);
    addChild(item.row, 0, int(items_.size()) * ROW_GAP);

    auto button_left = std::make_shared<Button>();
    button_left->setTexture("title", 104);
    item.row->addChild(button_left, ARROW_LEFT_OFFSET, 4);
    buttons_left_.push_back(button_left);

    auto button_right = std::make_shared<Button>();
    button_right->setTexture("title", 105);
    item.row->addChild(button_right, ARROW_RIGHT_OFFSET, 4);
    buttons_right_.push_back(button_right);

    items_.push_back(std::move(item));
}

UIConfig::OptionItem* UIConfig::findOption(const std::string& section, const std::string& key)
{
    for (auto& item : items_)
    {
        if (item.section == section && item.key == key)
        {
            return &item;
        }
    }
    return nullptr;
}

void UIConfig::loadConfig()
{
    for (auto& item : items_)
    {
        int value = GameUtil::getInstance()->getInt(item.section, item.key, 0);
        if (item.section == "music" && (item.key == "volume" || item.key == "volumewav"))
        {
            value /= 10;
        }
        item.value = clampIndex(value, int(item.options.size()));
    }
    if (auto item = findOption("music", "volume"))
    {
        item->value = clampIndex(GameUtil::getInstance()->getInt("music", "volume", 20) / 10, int(item->options.size()));
    }
    if (auto item = findOption("music", "volumewav"))
    {
        item->value = clampIndex(GameUtil::getInstance()->getInt("music", "volumewav", 50) / 10, int(item->options.size()));
    }
    int battle_mode = GameUtil::getInstance()->getInt("game", "battle_mode", 0);
    if (auto item = findOption("game", "battle_mode"))
    {
        item->value = clampIndex(battle_mode, int(item->options.size()));
        if (battle_mode != item->value)
        {
            GameUtil::getInstance()->setKey("game", "battle_mode", std::to_string(item->value));
        }
    }
    if (auto item = findOption("game", "battle_debug_win"))
    {
        item->value = battle_mode == -1 || GameUtil::getInstance()->getInt("game", "battle_debug_win", 0) != 0 ? 1 : 0;
    }
}

void UIConfig::saveConfig()
{
    for (auto& item : items_)
    {
        int value = item.value;
        if (item.section == "music" && (item.key == "volume" || item.key == "volumewav"))
        {
            value *= 10;
        }
        GameUtil::getInstance()->setKey(item.section, item.key, std::to_string(value));
    }
    if (auto item = findOption("music", "volume"))
    {
        Audio::getInstance()->setVolume(item->value * 10);
    }
    if (auto item = findOption("music", "volumewav"))
    {
        Audio::getInstance()->setVolumeWav(item->value * 10);
    }
    if (auto item = findOption("game", "simplified_chinese"))
    {
        Font::getInstance()->setSimplified(item->value);
    }
    if (auto item = findOption("game", "fullscreen"))
    {
        Engine::getInstance()->setFullScreen(item->value != 0);
    }
}

void UIConfig::refreshTexts()
{
    for (auto& item : items_)
    {
        item.row->setText(item.label);
    }
}

void UIConfig::changeOption(int index, int delta)
{
    if (index < 0 || index >= items_.size())
    {
        return;
    }
    auto& item = items_[index];
    item.value += delta;
    if (item.value < 0)
    {
        item.value = int(item.options.size()) - 1;
    }
    else if (item.value >= item.options.size())
    {
        item.value = 0;
    }
}

bool UIConfig::isConfigItemActive() const
{
    return active_child_ >= 0 && active_child_ < int(items_.size());
}

void UIConfig::draw()
{
    for (int i = 0; i < items_.size(); i++)
    {
        auto& item = items_[i];
        int row_x, row_y;
        item.row->getPosition(row_x, row_y);
        uint8_t alpha = item.row->getState() == NodeNormal ? 224 : 255;
        auto value = item.options[clampIndex(item.value, int(item.options.size()))];
        int value_width = 24 * Font::getTextDrawSize(value) / 2;
        int value_x = (ARROW_LEFT_OFFSET + ARROW_WIDTH + ARROW_RIGHT_OFFSET - value_width) / 2;
        auto rect = Font::getBoxRect(0, 24, row_x, row_y);
        int value_right = value_x + value_width;
        int arrow_right = ARROW_RIGHT_OFFSET + ARROW_WIDTH;
        rect.w = (std::max)(value_right, arrow_right) + 20;
        TextureManager::getInstance()->renderTexture("title", 126, rect.x, rect.y, { { 255, 255, 255, 255 }, alpha }, rect.w, rect.h);

        Color value_color = item.row->getState() == NodeNormal ? Color{ 48, 32, 16, 255 } : Color{ 255, 255, 255, 255 };
        Font::getInstance()->draw(value, 24, row_x + value_x, row_y, value_color, 255);
    }
}

void UIConfig::dealEvent(EngineEvent& e)
{
    static int first_press = 0;
    if (e.type == EVENT_KEY_DOWN && isConfigItemActive() && (e.key.key == K_LEFT || e.key.key == K_RIGHT))
    {
        if (first_press == 0 || first_press > 5)
        {
            changeOption(active_child_, e.key.key == K_LEFT ? -1 : 1);
        }
        first_press++;
    }
    else if (isConfigItemActive() && Engine::getInstance()->gameControllerGetButton(GAMEPAD_BUTTON_DPAD_LEFT))
    {
        changeOption(active_child_, -1);
        Engine::getInstance()->setInterValControllerPress(200);
    }
    else if (isConfigItemActive() && Engine::getInstance()->gameControllerGetButton(GAMEPAD_BUTTON_DPAD_RIGHT))
    {
        changeOption(active_child_, 1);
        Engine::getInstance()->setInterValControllerPress(200);
    }
    else
    {
        Menu::dealEvent(e);
    }

    if (e.type == EVENT_KEY_UP && (e.key.key == K_LEFT || e.key.key == K_RIGHT))
    {
        first_press = 0;
    }
}

void UIConfig::onPressedOK()
{
    for (int i = 0; i < items_.size(); i++)
    {
        if (buttons_left_[i]->getState() == NodePress)
        {
            changeOption(i, -1);
            return;
        }
        if (buttons_right_[i]->getState() == NodePress)
        {
            changeOption(i, 1);
            return;
        }
    }

    if (button_ok_->getState() == NodePress)
    {
        saveConfig();
        GameUtil::getInstance()->saveConfig();
        exitWithResult(-1);
        return;
    }
    if (button_cancel_->getState() == NodePress)
    {
        exitWithResult(-1);
        return;
    }

    checkActiveToResult();
    if (result_ >= 0 && result_ < int(items_.size()))
    {
        changeOption(result_, 1);
        result_ = -1;
    }
}