#pragma once
#include "TextBox.h"
#include "ChessUiStyle.h"
#include <cassert>
#include <optional>

struct ButtonSelectionOverlay
{
    Color outline{};
    Color fill{};
    int thickness = 1;
};

class ButtonVisualLayers
{
    std::optional<Color> custom_outline_;
    std::optional<ButtonSelectionOverlay> selection_overlay_;

public:
    void setCustomOutline(Color c) { custom_outline_ = c; }
    void clearCustomOutline() { custom_outline_.reset(); }
    bool hasCustomOutline() const { return custom_outline_.has_value(); }
    const std::optional<Color>& customOutline() const { return custom_outline_; }

    void setSelectionOverlay(Color outline, Color fill, int thickness)
    {
        assert(thickness > 0);
        selection_overlay_ = ButtonSelectionOverlay{outline, fill, thickness};
    }
    void clearSelectionOverlay() { selection_overlay_.reset(); }
    bool hasSelectionOverlay() const { return selection_overlay_.has_value(); }
    bool keepsTextColorDuringSelection() const { return hasSelectionOverlay(); }
    const std::optional<ButtonSelectionOverlay>& selectionOverlay() const { return selection_overlay_; }
};

class Button : public TextBox
{
    uint8_t alpha_ = 192;
    bool text_only_ = false;
    ButtonVisualLayers visual_layers_;
    bool animate_outline_ = false;
    int outline_thickness_ = 1;
    bool flat_style_ = false;
    bool flat_primary_{};
    bool flat_focused_{};
    std::optional<KysChess::ChessUiStyle::Icon> flat_icon_;

public:
    Button() { resize_with_text_ = true; }

    Button(const std::string& path, int normal_id, int pass_id = -1, int press_id = -1);

    virtual ~Button();

    //void InitMumber();
    void dealEvent(EngineEvent& e) override;
    PointerResult onPointerEvent(const PointerEvent& event) override;
    void draw() override;

    int getTexutreID() { return texture_normal_id_; }

    void onPressedOK() override { result_ = 0; }

    void setAlpha(uint8_t alpha) { alpha_ = alpha; }
    void setTextOnly(bool t) { text_only_ = t; }
    void setFlatStyle(int width, int height)
    {
        flat_style_ = true;
        text_only_ = false;
        resize_with_text_ = false;
        setSize(width, height);
        setTextPosition(14, (height - font_size_) / 2);
    }
    void setFlatPrimary(bool value) { flat_primary_ = value; }
    void setFlatFocused(bool value) { flat_focused_ = value; }
    void setFlatIcon(KysChess::ChessUiStyle::Icon icon)
    {
        flat_icon_ = icon;
        setTextPosition(48, text_y_);
    }
    void setCustomOutline(Color c) { visual_layers_.setCustomOutline(c); }
    void clearCustomOutline() { visual_layers_.clearCustomOutline(); }
    bool hasCustomOutline() const { return visual_layers_.hasCustomOutline(); }
    void setSelectionOverlay(Color outline, Color fill, int thickness) { visual_layers_.setSelectionOverlay(outline, fill, thickness); }
    void clearSelectionOverlay() { visual_layers_.clearSelectionOverlay(); }
    bool hasSelectionOverlay() const { return visual_layers_.hasSelectionOverlay(); }
    void setAnimateOutline(bool a) { animate_outline_ = a; }
    void setOutlineThickness(int t) { outline_thickness_ = t; }

};

class ButtonGetKey : public Button
{
public:
    virtual ~ButtonGetKey();
    void dealEvent(EngineEvent& e) override;

    virtual void onPressedOK() override {}

    virtual void onPressedCancel() override {}
};
