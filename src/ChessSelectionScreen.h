#pragma once

#include "RunNode.h"
#include "ChessUiStyle.h"
#include "ChessSelectionLayout.h"
#include "ChessCharacterPresentation.h"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace KysChess
{

struct ChessSelectionImage
{
    std::string group;
    int id = -1;
};

inline ChessSelectionImage chessSelectionIcon(ChessUiStyle::Icon icon)
{
    return {"chess-preparation", ChessUiStyle::iconId(icon)};
}

struct ChessOverviewRow
{
    std::string text;
    Color color = ChessUiStyle::ink;
    bool heading{};
    std::function<ChessCharacterPresentation()> inspect;
    std::function<std::vector<ChessCharacterEntry>()> synergies;
    std::optional<std::pair<std::string, Color>> prefix;
};

struct ChessOverviewColumn
{
    int width{};
    std::vector<ChessOverviewRow> rows;
};

struct ChessSelectionItem
{
    std::string title;
    std::string subtitle;
    std::string badge;
    ChessSelectionImage image;
    Color accent = ChessUiStyle::gold;
    std::string category;
    std::vector<std::pair<std::string, std::string>> facts;
    std::string summary;
    std::string relatedTitle;
    std::vector<ChessSelectionImage> related;
    std::string description;
    std::optional<ChessCharacterPresentation> character;
    std::optional<ChessTalentPresentation> talent;
    std::vector<std::pair<std::string, std::string>> detailTabs;
    std::vector<ChessOverviewColumn> overview;
    std::function<void(int)> openCombo;
};

// 棋局共用的卡片與檢視器；選取只更新預覽，明確操作才提交結果。
class ChessSelectionScreen : public RunNode
{
public:
    ChessSelectionScreen(std::string title, std::vector<ChessSelectionItem> items);
    void setShopMode(bool value) { shopMode_ = value; }
    void setBrowseOnly(bool value) { browseOnly_ = value; }
    void setCommandMode(bool value) { if (value) layout_ = ChessSelectionLayout::Command; }
    void setCompactChoices(bool value) { if (value) layout_ = ChessSelectionLayout::Dialog; }
    void setShortChoiceMode() { layout_ = ChessSelectionLayout::ShortChoice; }
    void setOverviewMode() { layout_ = ChessSelectionLayout::Overview; }
    void setTabbedMode() { layout_ = ChessSelectionLayout::Tabbed; browseOnly_ = true; }
    void setTalentMode() { layout_ = ChessSelectionLayout::Talent; }
    void setConfirmation(bool destructive = false) { dialogPrimary_ = 0; dialogDestructive_ = destructive ? 0 : -1; }
    void setActionLabel(std::string label) { actionLabel_ = std::move(label); }
    void setHeaderNote(std::string note) { headerNote_ = std::move(note); }
    void setExitable(bool value) { exitable_ = value; }
    void setExitConfirmation(std::function<bool()> confirmation) { exitConfirmation_ = std::move(confirmation); }
    void setSelectedItem(int index);
    void setSearchEnabled(bool value) { searchEnabled_ = value; }
    void setDetailRenderer(std::function<void(int, SDL_Rect)> renderer) { detailRenderer_ = std::move(renderer); }
    void onEntrance() override;
    void draw() override;
    void drawTopmostOverlay() override;
    void dealEvent(EngineEvent& event) override;
    PointerResult onPointerEvent(const PointerEvent& event) override;
    void onPointerInputReset() override { pressed_ = -1; }
    void onPressedOK() override;
    void onPressedCancel() override;
    uint64_t controlLayoutRevision() const override { return revision_; }

private:
    struct Control
    {
        SDL_Rect frame{};
        std::function<void()> activate;
        std::string tooltip;
        int itemIndex = -1;
    };
    std::string title_;
    std::string headerNote_;
    std::vector<ChessSelectionItem> items_;
    std::vector<int> visible_;
    std::vector<std::string> categories_;
    std::vector<Control> controls_;
    std::function<void(int, SDL_Rect)> detailRenderer_;
    std::function<bool()> exitConfirmation_;
    std::string actionLabel_ = "選擇";
    std::string category_;
    std::string query_;
    int selected_{};
    int page_{};
    int pressed_ = -1;
    int focused_{};
    int detailScroll_{};
    int detailTab_{};
    int detailScrollMax_{};
    int inspectedRow_ = -1;
    int inspectionOriginScroll_{};
    std::optional<ChessCharacterPresentation> inspection_;
    ChessCharacterInspectorState characterInspector_;
    int dialogPrimary_ = -1;
    int dialogDestructive_ = -1;
    bool keyboard_{};
    bool browseOnly_{};
    bool shopMode_{};
    ChessSelectionLayout layout_ = ChessSelectionLayout::Catalogue;
    bool compact_{};
    bool exitable_ = true;
    bool expanded_{};
    bool searchEnabled_{};
    uint64_t revision_{};
    int pageSize() const { return chessSelectionPageSize(layout_, compact_); }
    int columns() const { return chessSelectionColumns(layout_, compact_); }
    void updateCompact();
    void filter();
    void select(int index);
    void changePage(int step);
    void expand(bool value);
    void search();
    int hit(SDL_FPoint point) const;
    void control(SDL_Rect frame, std::function<void()> activate);
    void button(SDL_Rect frame, const std::string& text, std::function<void()> activate,
        bool primary = false, std::optional<ChessUiStyle::Icon> icon = std::nullopt, bool selected = false, bool action = false);
    void inspector();
    void closeInspection();
    void characterInspector(const ChessCharacterPresentation& character, SDL_Rect frame,
        std::optional<SDL_Rect> navigationFrame = std::nullopt);
    void drawChoice();
    void drawTalent();
    void drawOverview();
    void drawTabbed();
    void drawOverviewBody(SDL_Rect frame, int fontSize);
    void drawCompactCatalogue();
    std::string confirmationLabel() const;
};

} // namespace KysChess
