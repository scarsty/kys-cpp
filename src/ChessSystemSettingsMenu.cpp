#include "ChessSystemSettingsMenu.h"

#include "Audio.h"
#include "BattleScenePauseControl.h"
#include "Engine.h"
#include "Font.h"
#include "GameUtil.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <format>
#include <string>

namespace KysChess
{

namespace
{

constexpr int kVolumeStep = 5;
constexpr int kFooterBottomPadding = 48;
constexpr int kFooterDividerGap = 24;

std::string boolText(bool value)
{
    return value ? "開" : "關";
}

std::string languageText(bool simplifiedChinese)
{
    return simplifiedChinese ? "簡體" : "繁體";
}

Color textColorForStyle()
{
    return Engine::uiStyle() == 1 ? Color{240, 230, 210, 255} : Color{32, 32, 32, 255};
}

Color mutedTextColorForStyle()
{
    return Engine::uiStyle() == 1 ? Color{184, 174, 148, 255} : Color{76, 70, 56, 255};
}

}    // namespace

ChessSystemSettingsMenu::ChessSystemSettingsMenu(SystemSettingsData settings)
    : original_(settings)
    , settings_(settings)
{
    dark_ = 1;
}

int ChessSystemSettingsMenu::rowCount() const
{
    return static_cast<int>(Row::Count);
}

int ChessSystemSettingsMenu::rowIndex(Row row) const
{
    return static_cast<int>(row);
}

ChessSystemSettingsMenu::Row ChessSystemSettingsMenu::rowFromIndex(int index) const
{
    assert(index >= 0 && index < rowCount());
    return static_cast<Row>(index);
}

ChessSystemSettingsMenu::Layout ChessSystemSettingsMenu::layout() const
{
    auto* engine = Engine::getInstance();
    const int uiW = engine->getUIWidth();
    const int uiH = engine->getUIHeight();

    Layout out;
    out.panelW = std::min(760, uiW - 80);
    out.panelH = std::min(610, uiH - 60);
    out.panelX = (uiW - out.panelW) / 2;
    out.panelY = (uiH - out.panelH) / 2;
    out.rowX = out.panelX + 36;
    out.rowY = out.panelY + 82;
    out.rowW = out.panelW - 72;
    out.rowH = 44;
    out.rowGap = 8;
    out.labelX = out.rowX + 18;
    out.valueX = out.rowX + out.rowW - 150;
    out.sliderX = out.rowX + 270;
    out.sliderW = std::max(180, out.rowW - 430);
    out.actionButtonGap = 24;
    out.actionButtonW = (out.rowW - out.actionButtonGap) / 2;
    out.actionDoneX = out.rowX;
    out.actionCancelX = out.actionDoneX + out.actionButtonW + out.actionButtonGap;
    out.actionY = settingsFooterActionY(out.panelY, out.panelH, out.rowH, kFooterBottomPadding);
    return out;
}

int ChessSystemSettingsMenu::rowTop(const Layout& layout, Row row) const
{
    if (isActionRow(row))
    {
        return layout.actionY;
    }
    return layout.rowY + rowIndex(row) * (layout.rowH + layout.rowGap);
}

std::optional<ChessSystemSettingsMenu::Row> ChessSystemSettingsMenu::rowAt(const Layout& layout, int x, int y) const
{
    if (y >= layout.actionY && y <= layout.actionY + layout.rowH)
    {
        if (x >= layout.actionDoneX && x <= layout.actionDoneX + layout.actionButtonW)
        {
            return Row::Done;
        }
        if (x >= layout.actionCancelX && x <= layout.actionCancelX + layout.actionButtonW)
        {
            return Row::Cancel;
        }
    }

    if (x < layout.rowX || x > layout.rowX + layout.rowW)
    {
        return std::nullopt;
    }

    for (int i = 0; i < rowCount(); ++i)
    {
        const auto row = rowFromIndex(i);
        if (isActionRow(row))
        {
            continue;
        }
        const int top = rowTop(layout, row);
        if (y >= top && y <= top + layout.rowH)
        {
            return row;
        }
    }
    return std::nullopt;
}

bool ChessSystemSettingsMenu::isVolumeRow(Row row) const
{
    return row == Row::MusicVolume || row == Row::SoundVolume;
}

bool ChessSystemSettingsMenu::isActionRow(Row row) const
{
    return row == Row::Done || row == Row::Cancel;
}

int ChessSystemSettingsMenu::actionX(const Layout& layout, Row row) const
{
    assert(isActionRow(row));
    return row == Row::Done ? layout.actionDoneX : layout.actionCancelX;
}

int& ChessSystemSettingsMenu::volumeForRow(Row row)
{
    assert(isVolumeRow(row));
    return row == Row::MusicVolume ? settings_.musicVolume : settings_.soundVolume;
}

int ChessSystemSettingsMenu::volumeForRow(Row row) const
{
    assert(isVolumeRow(row));
    return row == Row::MusicVolume ? settings_.musicVolume : settings_.soundVolume;
}

void ChessSystemSettingsMenu::moveSelection(int delta)
{
    const int next = (rowIndex(selectedRow_) + delta + rowCount()) % rowCount();
    selectedRow_ = rowFromIndex(next);
}

void ChessSystemSettingsMenu::adjustSelectedRow(int delta)
{
    if (isVolumeRow(selectedRow_))
    {
        volumeForRow(selectedRow_) = adjustSettingsSliderValue(volumeForRow(selectedRow_), delta * kVolumeStep);
        applyPreview();
        return;
    }

    if (selectedRow_ == Row::BattleSpeed && delta != 0)
    {
        settings_.battleSpeed = nextBattleSpeedSetting(settings_.battleSpeed);
        applyPreview();
    }
}

void ChessSystemSettingsMenu::activateSelectedRow()
{
    switch (selectedRow_)
    {
    case Row::ManualCamera:
        settings_.manualCamera = !settings_.manualCamera;
        applyPreview();
        break;
    case Row::ShowBattleLog:
        settings_.showBattleLog = !settings_.showBattleLog;
        applyPreview();
        break;
    case Row::PaperBattleView:
        settings_.paperBattleView = !settings_.paperBattleView;
        applyPreview();
        break;
    case Row::DebugLatencyLog:
        settings_.debugLatencyLog = !settings_.debugLatencyLog;
        applyPreview();
        break;
    case Row::MusicVolume:
    case Row::SoundVolume:
        break;
    case Row::BattleSpeed:
        settings_.battleSpeed = nextBattleSpeedSetting(settings_.battleSpeed);
        applyPreview();
        break;
    case Row::Language:
        settings_.simplifiedChinese = !settings_.simplifiedChinese;
        applyPreview();
        break;
    case Row::Done:
        exitWithResult(0);
        break;
    case Row::Cancel:
        restorePreview();
        exitWithResult(-1);
        break;
    case Row::Count:
        assert(false);
        break;
    }
}

void ChessSystemSettingsMenu::startSliderDrag(Row row, int pointerX)
{
    assert(isVolumeRow(row));
    draggingSlider_ = row;
    updateSliderDrag(pointerX);
}

void ChessSystemSettingsMenu::updateSliderDrag(int pointerX)
{
    if (!draggingSlider_)
    {
        return;
    }

    const auto currentLayout = layout();
    volumeForRow(*draggingSlider_) = settingsSliderValueFromPointer(pointerX, currentLayout.sliderX, currentLayout.sliderW);
    applyPreview();
}

void ChessSystemSettingsMenu::stopSliderDrag()
{
    draggingSlider_.reset();
}

void ChessSystemSettingsMenu::applyPreview()
{
    SystemSettings::getInstance()->update(settings_, false);
}

void ChessSystemSettingsMenu::restorePreview()
{
    SystemSettings::getInstance()->update(original_, false);
}

void ChessSystemSettingsMenu::dealEvent(EngineEvent& e)
{
    if (inputGuardFrames_ > 0)
    {
        if (isPressOK(e))
        {
            suppressNextOk_ = true;
        }
        if (isPressCancel(e))
        {
            suppressNextCancel_ = true;
        }
        --inputGuardFrames_;
        return;
    }

    if (e.type == EVENT_KEY_DOWN)
    {
        switch (e.key.key)
        {
        case K_UP:
        case K_W:
            moveSelection(-1);
            break;
        case K_DOWN:
        case K_S:
            moveSelection(1);
            break;
        case K_LEFT:
        case K_A:
            adjustSelectedRow(-1);
            break;
        case K_RIGHT:
        case K_D:
            adjustSelectedRow(1);
            break;
        default:
            break;
        }
    }

    if (e.type == EVENT_GAMEPAD_BUTTON_DOWN)
    {
        switch (e.gbutton.button)
        {
        case GAMEPAD_BUTTON_DPAD_UP:
            moveSelection(-1);
            break;
        case GAMEPAD_BUTTON_DPAD_DOWN:
            moveSelection(1);
            break;
        case GAMEPAD_BUTTON_DPAD_LEFT:
            adjustSelectedRow(-1);
            break;
        case GAMEPAD_BUTTON_DPAD_RIGHT:
            adjustSelectedRow(1);
            break;
        default:
            break;
        }
    }
}

RunNode::PointerResult ChessSystemSettingsMenu::onPointerEvent(const PointerEvent& event)
{
    if (inputGuardFrames_ > 0)
    {
        return PointerResult::Handled;
    }
    const auto currentLayout = layout();
    const int x = static_cast<int>(std::lround(event.uiPosition.x));
    const int y = static_cast<int>(std::lround(event.uiPosition.y));
    if (event.phase == PointerPhase::ButtonDown && event.button == SDL_BUTTON_LEFT)
    {
        if (!event.insidePresent) return PointerResult::Ignored;
        pointerDownRow_ = rowAt(currentLayout, x, y);
        if (!pointerDownRow_)
        {
            return PointerResult::Ignored;
        }
        selectedRow_ = *pointerDownRow_;
        if (isVolumeRow(*pointerDownRow_))
        {
            startSliderDrag(*pointerDownRow_, x);
        }
        return PointerResult::Captured;
    }
    if (event.phase == PointerPhase::Motion)
    {
        if (draggingSlider_)
        {
            updateSliderDrag(x);
        }
        else if (auto row = rowAt(currentLayout, x, y))
        {
            selectedRow_ = *row;
        }
        return PointerResult::Handled;
    }
    if (event.phase == PointerPhase::ButtonUp && event.button == SDL_BUTTON_LEFT)
    {
        if (draggingSlider_)
        {
            updateSliderDrag(x);
            stopSliderDrag();
        }
        else if (pointerDownRow_)
        {
            const auto upRow = rowAt(currentLayout, x, y);
            if (upRow && *upRow == *pointerDownRow_)
            {
                selectedRow_ = *upRow;
                activateSelectedRow();
            }
        }
        pointerDownRow_.reset();
        return PointerResult::Handled;
    }
    if (event.phase == PointerPhase::Cancel)
    {
        stopSliderDrag();
        pointerDownRow_.reset();
        return PointerResult::Handled;
    }
    return PointerResult::Ignored;
}

void ChessSystemSettingsMenu::onPressedOK()
{
    if (suppressNextOk_)
    {
        suppressNextOk_ = false;
        return;
    }
    activateSelectedRow();
}

void ChessSystemSettingsMenu::onPressedCancel()
{
    if (suppressNextCancel_)
    {
        suppressNextCancel_ = false;
        return;
    }
    restorePreview();
    exitWithResult(-1);
}

void ChessSystemSettingsMenu::draw()
{
    const auto currentLayout = layout();
    auto* engine = Engine::getInstance();
    auto* font = Font::getInstance();

    const Color panelBg = Engine::uiStyle() == 1 ? Color{16, 18, 16, 226} : Color{235, 226, 196, 232};
    const Color panelBorder = Engine::uiStyle() == 1 ? Color{190, 166, 96, 220} : Color{86, 62, 38, 220};
    const Color selectedBg = Engine::uiStyle() == 1 ? Color{76, 70, 42, 180} : Color{220, 196, 128, 190};
    const Color rowBorder = Engine::uiStyle() == 1 ? Color{112, 98, 64, 160} : Color{116, 86, 52, 160};
    const Color trackBg = Engine::uiStyle() == 1 ? Color{44, 48, 43, 230} : Color{176, 160, 126, 230};
    const Color trackFill = Color{216, 180, 82, 255};
    const Color knobColor = Color{246, 226, 132, 255};
    const Color titleColor = Color{238, 221, 112, 255};
    const Color textColor = textColorForStyle();
    const Color mutedTextColor = mutedTextColorForStyle();
    const Color dividerColor = Engine::uiStyle() == 1 ? Color{128, 112, 72, 170} : Color{124, 92, 54, 150};

    engine->fillRoundedRect(panelBg, currentLayout.panelX, currentLayout.panelY, currentLayout.panelW, currentLayout.panelH, 12);
    engine->drawRoundedRect(panelBorder, currentLayout.panelX, currentLayout.panelY, currentLayout.panelW, currentLayout.panelH, 12);

    font->draw("系統設定", 34, currentLayout.panelX + 36, currentLayout.panelY + 28, titleColor);
    engine->fillRoundedRect(dividerColor, currentLayout.rowX, currentLayout.actionY - kFooterDividerGap, currentLayout.rowW, 2, 1);

    const std::array<std::string, static_cast<int>(Row::Count)> labels = {
        "手動鏡頭",
        "顯示戰鬥日誌",
        "紙片3D戰場",
        "偵錯耗時日誌",
        "音樂",
        "音效",
        "戰鬥速度",
        "文字",
        "完成",
        "取消"};

    for (int i = 0; i < rowCount(); ++i)
    {
        const Row row = rowFromIndex(i);
        const int top = rowTop(currentLayout, row);
        const bool actionRow = isActionRow(row);
        const int itemX = actionRow ? actionX(currentLayout, row) : currentLayout.rowX;
        const int itemW = actionRow ? currentLayout.actionButtonW : currentLayout.rowW;
        if (row == selectedRow_)
        {
            engine->fillRoundedRect(selectedBg, itemX, top, itemW, currentLayout.rowH, 8);
            engine->drawRoundedRect(rowBorder, itemX, top, itemW, currentLayout.rowH, 8);
        }

        if (actionRow)
        {
            constexpr int fontSize = 26;
            const int textW = fontSize * Font::getTextDrawSize(labels[i]) / 2;
            font->draw(labels[i], fontSize, itemX + (itemW - textW) / 2, top + 9, textColor);
            continue;
        }

        font->draw(labels[i], 26, currentLayout.labelX, top + 9, textColor);

        if (isVolumeRow(row))
        {
            const int value = volumeForRow(row);
            const int trackY = top + currentLayout.rowH / 2 - 4;
            const int fillW = currentLayout.sliderW * value / 100;
            const int knobRadius = 11;
            const int knobCenterX = currentLayout.sliderX + fillW;
            engine->fillRoundedRect(trackBg, currentLayout.sliderX, trackY, currentLayout.sliderW, 8, 4);
            engine->fillRoundedRect(trackFill, currentLayout.sliderX, trackY, fillW, 8, 4);
            engine->fillRoundedRect(knobColor, knobCenterX - knobRadius, trackY - 7, knobRadius * 2, knobRadius * 2, knobRadius);
            font->draw(std::format("{}%", value), 24, currentLayout.valueX, top + 10, mutedTextColor);
            continue;
        }

        std::string value;
        Color valueColor = mutedTextColor;
        switch (row)
        {
        case Row::ManualCamera:
            value = boolText(settings_.manualCamera);
            valueColor = settingsToggleValueColor(settings_.manualCamera);
            break;
        case Row::ShowBattleLog:
            value = boolText(settings_.showBattleLog);
            valueColor = settingsToggleValueColor(settings_.showBattleLog);
            break;
        case Row::PaperBattleView:
            value = boolText(settings_.paperBattleView);
            valueColor = settingsToggleValueColor(settings_.paperBattleView);
            break;
        case Row::DebugLatencyLog:
            value = boolText(settings_.debugLatencyLog);
            valueColor = settingsToggleValueColor(settings_.debugLatencyLog);
            break;
        case Row::BattleSpeed:
            value = std::string(battleSpeedDisplayText(settings_.battleSpeed));
            break;
        case Row::Language:
            value = languageText(settings_.simplifiedChinese);
            break;
        case Row::Done:
        case Row::Cancel:
            value = "";
            break;
        case Row::MusicVolume:
        case Row::SoundVolume:
        case Row::Count:
            assert(false);
            break;
        }
        if (!value.empty())
        {
            font->draw(value, 26, currentLayout.valueX, top + 9, valueColor);
        }
    }
}

}    // namespace KysChess
