#pragma once

#include "Button.h"
#include "Menu.h"

#include <string>
#include <vector>

class UIConfig : public Menu
{
public:
    UIConfig();
    ~UIConfig() override;

    void dealEvent(EngineEvent& e) override;
    void draw() override;
    void onPressedOK() override;
    void onPressedCancel() override { exitWithResult(-1); }

private:
    struct OptionItem
    {
        std::string label;
        std::string section;
        std::string key;
        std::vector<std::string> options;
        int value = 0;
        std::shared_ptr<Button> row;
    };

    std::vector<OptionItem> items_;
    std::vector<std::shared_ptr<Button>> buttons_left_;
    std::vector<std::shared_ptr<Button>> buttons_right_;
    std::shared_ptr<Button> button_ok_;
    std::shared_ptr<Button> button_cancel_;

    static constexpr int ROW_WIDTH = 180;
    static constexpr int ROW_HEIGHT = 28;
    static constexpr int ROW_GAP = 36;
    static constexpr int ARROW_WIDTH = 16;
    static constexpr int ARROW_LEFT_OFFSET = 150;
    static constexpr int ARROW_RIGHT_OFFSET = 250;
    static constexpr int BUTTON_TOP_GAP = 18;

    void addOption(const std::string& label, const std::string& section, const std::string& key, const std::vector<std::string>& options);
    void loadConfig();
    void saveConfig();
    void refreshTexts();
    void changeOption(int index, int delta);
    OptionItem* findOption(const std::string& section, const std::string& key);
    bool isConfigItemActive() const;
    static int clampIndex(int value, int count);
    static std::vector<std::string> createVolumeOptions();
};