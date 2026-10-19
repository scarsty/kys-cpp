#include "ChessSystemSettingsMenu.h"

#include "Audio.h"
#include "BattleScenePauseControl.h"
#include "Engine.h"
#include "Font.h"
#include "ChessUiStyle.h"
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
    return ChessUiStyle::ink;
}

Color mutedTextColorForStyle()
{
    return ChessUiStyle::muted;
}

}    // namespace

ChessSystemSettingsMenu::ChessSystemSettingsMenu(SystemSettingsData settings)
    : original_(settings)
    , settings_(settings)
{
    dark_ = 0;
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
    out.panelH = std::min(Engine::supportsDesktopFullscreen() ? 660 : 610, uiH - 60);
    out.panelX = (uiW - out.panelW) / 2;
    out.panelY = (uiH - out.panelH) / 2;
    out.rowX = out.panelX + 36;
    out.rowY = out.panelY + 82;
    out.rowW = out.panelW - 72;
    out.rowGap = 8;
    out.rowH = settingsRowHeight(out.panelH, rowIndex(Row::Done), out.rowGap, kFooterBottomPadding, kFooterDividerGap);
    out.labelX = out.rowX + 18;
    out.valueX = out.rowX + out.rowW - 150;
    out.sliderX = out.rowX + 270;
    out.sliderW = std::max(180, out.rowW - 430);
    out.actionButtonGap = 24;
    out.actionButtonW = (out.rowW - out.actionButtonGap) / 2;
    const auto footer = ChessUiStyle::actionFooter({out.rowX - 16, out.panelY,
        out.rowW + 32, out.panelH - kFooterBottomPadding + 16}, out.actionButtonW, out.rowH);
    out.actionCancelX = footer.leading.x;
    out.actionDoneX = footer.trailing.x;
    out.actionY = footer.leading.y;
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
        const int left = layout.rowX;
        if (x >= left && x <= left + layout.rowW && y >= top && y <= top + layout.rowH)
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
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    case Row::BorderlessFullscreen:
        onToggleFullscreen();
        break;
#endif
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
    volumeForRow(*draggingSlider_) = settingsSliderValueFromPointer(
        pointerX, currentLayout.sliderX, currentLayout.sliderW);
    applyPreview();
}

void ChessSystemSettingsMenu::stopSliderDrag()
{
    draggingSlider_.reset();
}

void ChessSystemSettingsMenu::applyPreview()
{
    SystemSettings::getInstance()->update(settings_, false);
    settings_ = SystemSettings::getInstance()->snapshot();
}

void ChessSystemSettingsMenu::onToggleFullscreen()
{
    settings_.borderlessFullscreen = !settings_.borderlessFullscreen;
    applyPreview();
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
        keyboardFocus_ = true;
        switch (e.key.key)
        {
        case K_UP:
        case K_W:
            moveSelection(-1);
            break;
        case SDLK_TAB:
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
        keyboardFocus_ = true;
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
    if (event.phase == PointerPhase::Motion || event.phase == PointerPhase::ButtonDown) keyboardFocus_ = false;
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

    using namespace ChessUiStyle;
    const Color trackBg{59, 54, 45, 230};
    const Color trackFill = gold;
    const Color knobColor = ink;
    const Color textColor = textColorForStyle();
    const Color mutedTextColor = mutedTextColorForStyle();
    const Color dividerColor = line;

    engine->fillColor(canvasScrim, 0, 0, engine->getUIWidth(), engine->getUIHeight());
    modalShell({currentLayout.panelX, currentLayout.panelY, currentLayout.panelW, currentLayout.panelH}, "系統設定");
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
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
        "無邊框全螢幕 (F10)",
#endif
        "完成",
        "取消"};

    for (int i = 0; i < rowCount(); ++i)
    {
        const Row row = rowFromIndex(i);
        const int top = rowTop(currentLayout, row);
        const bool actionRow = isActionRow(row);
        const int itemX = actionRow ? actionX(currentLayout, row) : currentLayout.rowX;
        const int itemW = actionRow ? currentLayout.actionButtonW : currentLayout.rowW;
        const auto pointer = PointerInput::instance().logicalPointerUiPosition();
        const bool hovered = pointer.x >= itemX && pointer.x < itemX + itemW && pointer.y >= top && pointer.y < top + currentLayout.rowH;
        controlSurface({itemX, top, itemW, currentLayout.rowH}, {.hovered = hovered,
            .focused = keyboardFocus_ && row == selectedRow_, .primary = row == Row::Done});

        if (actionRow)
        {
            const int fontSize = Type::sectionTitle;
            const int textW = fontSize * Font::getTextDrawSize(labels[i]) / 2;
            font->draw(labels[i], fontSize, itemX + (itemW - textW) / 2,
                top + (currentLayout.rowH - fontSize) / 2, row == Row::Done ? darkLabel : textColor);
            continue;
        }

        const int fontSize = Type::sectionTitle;
        const int textY = top + (currentLayout.rowH - fontSize) / 2;
        font->draw(labels[i], fontSize,
            currentLayout.labelX, textY, textColor);

        if (isVolumeRow(row))
        {
            const int value = volumeForRow(row);
            const int trackY = top + currentLayout.rowH / 2 - 4;
            const int fillW = currentLayout.sliderW * value / 100;
            const int knobRadius = 11;
            const int trackX = currentLayout.sliderX;
            const int knobCenterX = trackX + fillW;
            engine->fillRoundedRect(trackBg, trackX, trackY, currentLayout.sliderW, 8, 4);
            engine->fillRoundedRect(trackFill, trackX, trackY, fillW, 8, 4);
            engine->fillRoundedRect(knobColor, knobCenterX - knobRadius, trackY - 7, knobRadius * 2, knobRadius * 2, knobRadius);
            font->draw(std::format("{}%", value), fontSize,
                currentLayout.valueX, textY, mutedTextColor);
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
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
        case Row::BorderlessFullscreen:
            value = boolText(settings_.borderlessFullscreen);
            valueColor = settingsToggleValueColor(settings_.borderlessFullscreen);
            break;
#endif
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
            const int controlW = 136;
            const int controlX = itemX + itemW - controlW - 12;
            if (row == Row::BattleSpeed || row == Row::Language)
            {
                const int iconSize = 20;
                controlSurface({controlX, top + 4, controlW, currentLayout.rowH - 8}, {});
                asset("chess-preparation", iconId(Icon::Refresh), {controlX + controlW - iconSize - 8, top + (currentLayout.rowH - iconSize) / 2, iconSize, iconSize});
                font->draw(value, fontSize, controlX + 10, textY, textColor);
            }
            else
            {
                const bool enabled = value == "開";
                const int h = 28;
                const int w = 56;
                const int switchX = controlX + controlW - w;
                const int switchY = top + (currentLayout.rowH - h) / 2;
                engine->fillRoundedRect(enabled ? actionHoverFill : trackBg, switchX, switchY, w, h, h / 2);
                engine->fillRoundedRect(enabled ? gold : muted, switchX + (enabled ? w - h + 3 : 3), switchY + 3, h - 6, h - 6, (h - 6) / 2);
                font->draw(value, fontSize, controlX + 4, textY, valueColor);
            }
        }
    }
}

}    // namespace KysChess
