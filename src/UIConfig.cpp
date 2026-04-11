#include "UIConfig.h"

#include "Audio.h"
#include "Font.h"
#include "GameUtil.h"
#include "TextureManager.h"

#include <algorithm>

namespace {
    constexpr int ROW_WIDTH = 180;
    constexpr int ROW_HEIGHT = 28;
    constexpr int ROW_GAP = 36;
    constexpr int ARROW_LEFT_OFFSET = 140;
    constexpr int VALUE_OFFSET = 162;
    constexpr int ARROW_RIGHT_OFFSET = 250;
    constexpr int BUTTON_Y_OFFSET = 250;

    int clampIndex(int value, int count)
    {
        if (count <= 0) {
            return 0;
        }
        return (std::max)(0, (std::min)(value, count - 1));
    }

    std::vector<std::string> createVolumeOptions()
    {
        std::vector<std::string> options;
        for (int value = 0; value <= 100; value += 10)
        {
            options.push_back(std::to_string(value));
        }
        return options;
    }
}

UIConfig::UIConfig()
{
    auto volume_options = createVolumeOptions();
    addOption("音樂音量", "music", "volume", volume_options);
    addOption("戰鬥音效", "music", "volumewav", volume_options);
    addOption("戰鬥模式", "game", "battle_mode", { "回合制", "半即時", "黑帝斯", "隻狼", "紙片" });
    addOption("格擋輔助", "game", "easy_block", { "關閉", "開啟" });
    addOption("直接勝利", "game", "battle_debug_win", { "關閉", "開啟" });
    addOption("文字設置", "game", "simplified_chinese", { "繁體", "簡體" });

    button_ok_ = std::make_shared<Button>();
    button_ok_->setFontSize(24);
    button_ok_->setText("確認");
    addChild(button_ok_, 100, BUTTON_Y_OFFSET);

    button_cancel_ = std::make_shared<Button>();
    button_cancel_->setFontSize(24);
    button_cancel_->setText("取消");
    addChild(button_cancel_, 200, BUTTON_Y_OFFSET);

    w_ = ROW_WIDTH;
    h_ = BUTTON_Y_OFFSET + 30;

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

void UIConfig::loadConfig()
{
    items_[0].value = clampIndex(GameUtil::getInstance()->getInt("music", "volume", 20) / 10, int(items_[0].options.size()));
    items_[1].value = clampIndex(GameUtil::getInstance()->getInt("music", "volumewav", 50) / 10, int(items_[1].options.size()));

    int battle_mode = GameUtil::getInstance()->getInt("game", "battle_mode", 0);
    bool debug_win = battle_mode == -1 || GameUtil::getInstance()->getInt("game", "battle_debug_win", 0) != 0;
    items_[2].value = clampIndex(battle_mode, int(items_[2].options.size()));
    items_[3].value = GameUtil::getInstance()->getInt("game", "easy_block", 0) ? 1 : 0;
    items_[4].value = debug_win ? 1 : 0;
    items_[5].value = GameUtil::getInstance()->getInt("game", "simplified_chinese", 0) ? 1 : 0;
}

void UIConfig::saveConfig()
{
    GameUtil::getInstance()->setKey("music", "volume", std::to_string(items_[0].value * 10));
    GameUtil::getInstance()->setKey("music", "volumewav", std::to_string(items_[1].value * 10));
    GameUtil::getInstance()->setKey("game", "battle_mode", std::to_string(items_[2].value));
    GameUtil::getInstance()->setKey("game", "easy_block", std::to_string(items_[3].value));
    GameUtil::getInstance()->setKey("game", "battle_debug_win", std::to_string(items_[4].value));
    GameUtil::getInstance()->setKey("game", "simplified_chinese", std::to_string(items_[5].value));

    Audio::getInstance()->setVolume(items_[0].value * 10);
    Audio::getInstance()->setVolumeWav(items_[1].value * 10);
    Font::getInstance()->setSimplified(items_[5].value);
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
    if (index < 0 || index >= items_.size()) {
        return;
    }
    auto& item = items_[index];
    item.value += delta;
    if (item.value < 0) {
        item.value = int(item.options.size()) - 1;
    }
    else if (item.value >= item.options.size()) {
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
        TextureManager::getInstance()->renderTexture("title", 126, row_x+120, row_y, { { 255, 255, 255, 255 }, alpha }, ROW_WIDTH, ROW_HEIGHT);

        Color value_color = item.row->getState() == NodeNormal ? Color{ 48, 32, 16, 255 } : Color{ 255, 255, 255, 255 };
        Font::getInstance()->draw(item.options[clampIndex(item.value, int(item.options.size()))], 24, row_x + VALUE_OFFSET, row_y, value_color, 255);
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
        exitWithResult(0);
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