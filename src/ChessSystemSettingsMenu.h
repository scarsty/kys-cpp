#pragma once

#include "RunNode.h"
#include "SystemSettings.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <optional>

namespace KysChess
{

inline int settingsSliderValueFromPointer(int pointerX, int trackX, int trackWidth)
{
    assert(trackWidth > 0);
    const int relative = std::clamp(pointerX - trackX, 0, trackWidth);
    return static_cast<int>(std::lround(relative * 100.0 / trackWidth));
}

inline int adjustSettingsSliderValue(int value, int delta)
{
    return std::clamp(value + delta, 0, 100);
}

inline Color settingsToggleValueColor(bool enabled)
{
    return enabled ? Color{0, 255, 100, 255} : Color{255, 80, 80, 255};
}

inline int settingsFooterActionY(int panelY, int panelH, int rowH, int bottomPadding)
{
    return panelY + panelH - bottomPadding - rowH;
}

class ChessSystemSettingsMenu : public RunNode
{
public:
    explicit ChessSystemSettingsMenu(SystemSettingsData settings);

    const SystemSettingsData& settings() const { return settings_; }

    void draw() override;
    void dealEvent(EngineEvent& e) override;
    void onPressedOK() override;
    void onPressedCancel() override;

private:
    enum class Row
    {
        ManualCamera,
        ShowBattleLog,
        DebugLatencyLog,
        MusicVolume,
        SoundVolume,
        BattleSpeed,
        Language,
        Done,
        Cancel,
        Count
    };

    struct PointerPosition
    {
        int x{};
        int y{};
    };

    struct Layout
    {
        int panelX{};
        int panelY{};
        int panelW{};
        int panelH{};
        int rowX{};
        int rowY{};
        int rowW{};
        int rowH{};
        int rowGap{};
        int labelX{};
        int valueX{};
        int sliderX{};
        int sliderW{};
        int actionY{};
        int actionButtonW{};
        int actionButtonGap{};
        int actionDoneX{};
        int actionCancelX{};
    };

    Layout layout() const;
    int rowCount() const;
    int rowIndex(Row row) const;
    Row rowFromIndex(int index) const;
    int rowTop(const Layout& layout, Row row) const;
    std::optional<Row> rowAt(const Layout& layout, int x, int y) const;
    std::optional<PointerPosition> pointerPosition(const EngineEvent& e) const;
    bool isActionRow(Row row) const;
    int actionX(const Layout& layout, Row row) const;
    bool isVolumeRow(Row row) const;
    int& volumeForRow(Row row);
    int volumeForRow(Row row) const;
    void moveSelection(int delta);
    void adjustSelectedRow(int delta);
    void activateSelectedRow();
    void startSliderDrag(Row row, int pointerX);
    void updateSliderDrag(int pointerX);
    void stopSliderDrag();
    void applyPreview();
    void restorePreview();

    SystemSettingsData original_;
    SystemSettingsData settings_;
    Row selectedRow_ = Row::ManualCamera;
    std::optional<Row> draggingSlider_;
    int inputGuardFrames_ = 8;
    bool suppressNextOk_ = false;
    bool suppressNextCancel_ = false;
};

}    // namespace KysChess
