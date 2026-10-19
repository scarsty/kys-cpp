#pragma once

#include "ChessContextMenu.h"
#include "ChessCharacterPresentation.h"
#include "ChessGameSession.h"
#include "ChessPreparationModel.h"
#include "ChessScreenLayout.h"
#include "ChessUiCommon.h"
#include "RunNode.h"

#include <functional>
#include <string>

namespace KysChess
{

struct ChessCharacterEntry;

struct ChessPreparationCallbacks
{
    std::function<ChessActionResult(const ChessAction&)> submit;
};

class ChessPreparationScreen : public RunNode
{
public:
    ChessPreparationScreen(ChessGameSession& session, ChessPreparationCallbacks callbacks);
    std::optional<ChessContextMenuAction> takeCommand();
    void onEntrance() override;
    void draw() override;
    void drawTopmostOverlay() override;
    void dealEvent(EngineEvent& event) override;
    PointerResult onPointerEvent(const PointerEvent& event) override;
    void onPointerInputReset() override;
    void onPressedOK() override;
    void onPressedCancel() override;
    void onPressedContextMenu() override;
    uint64_t controlLayoutRevision() const override { return layoutRevision_; }

private:
    using View = ChessPreparationView;
    using Icon = ChessUiStyle::Icon;
    enum class Treatment { Quiet, Card, Slot, Action, Primary, Row };
    struct Control
    {
        std::string key;
        PanelFrame frame{};
        std::string tooltip;
        std::function<void()> activate;
        bool enabled = true;
        bool touchConfirmation = false;
        std::function<void()> preview;
        std::function<void()> shortcut;
        int equipmentId = -1;
        bool tooltipBelow{};
        std::vector<PanelTextSourceRow> tooltipRows;
        std::string confirmationLabel;
    };
    ChessGameSession& session_;
    ChessPreparationCallbacks callbacks_;
    std::optional<ChessContextMenuAction> pendingCommand_;
    ChessGameplayObservation observation_;
    ChessPreparationSelection selection_;
    ChessCharacterInspectorState characterInspector_;
    View view_ = View::Piece;
    ChessPreparationHistory history_;
    std::vector<Control> controls_;
    std::vector<std::function<void()>> drawings_;
    std::size_t guideDrawingStart_{};
    struct PieceCard
    {
        ChessSessionPiece piece;
        PanelFrame frame{};
        bool compact{};
    };
    ChessPreparationRosterOrder rosterOrder_;
    std::vector<PieceCard> pieceCards_;
    std::map<int, ChessPreparationCardMotion> cardMotions_;
    ChessPreparationEquipmentPopover equipmentPopover_;
    PanelFrame popoverFrame_{};
    PanelFrame popoverClose_{};
    int popoverScrollMax_{};
    double popoverLeaveSince_{};
    bool popoverDragging_{};
    int dismissedPopoverId_ = -1;
    PanelFrame team_{};
    PanelFrame bench_{};
    PanelFrame shop_{};
    PanelFrame inspector_{};
    PanelFrame footer_{};
    std::string focusKey_;
    std::string pressedKey_;
    std::string hoverKey_;
    double hoverSince_{};
    bool keyboardFocus_ = false;
    bool touchInput_{};
    ChessPreparationConfirmation confirmation_;
    ChessPortraitClicks portraitClicks_;
    ChessPressPreview pressPreview_;
    std::string touchHint_;
    std::string notice_;
    PanelFrame noticeAnchor_{};
    double noticeUntil_{};
    std::vector<int> arrivalIds_;
    double arrivalUntil_{};
    bool arrivalMerged_{};
    struct LuckFeedback
    {
        int instanceId{};
        int stacks{};
        double until{};
    };
    std::vector<LuckFeedback> luckFeedback_;
    bool singleClickMovement_{};
    int guideStep_ = -1;
    bool benchNames_ = false;
    int benchPage_{};
    int shopPage_{};
    int listPage_{};
    int textPage_{};
    int equipmentId_ = -1;
    int equipmentType_{};
    int abilityId_ = -1;
    int neigongId_ = -1;
    std::string information_;
    uint64_t layoutRevision_{};

    void rebuild();
    void header();
    void roster();
    void shop();
    void inspector();
    void pieceInspector();
    void equipmentInspector();
    void informationInspector();
    void expandedInspector();
    void footer();
    void guide();
    void finishGuide();
    bool guideAllows(const std::string& key) const;
    void navigate(View view, int detailId = -1, std::string information = {});
    void back();
    void selectPiece(int id);
    void selectShop(int slot);
    void open(ChessContextMenuAction command);
    ChessActionResult submit(const ChessAction& action, const std::string& message);
    void buy(int slot);
    void pickEquipment(int type);
    void ban(int roleId);
    void deploy();
    void activate(std::string key, bool touch = false);
    int hit(SDL_FPoint point) const;
    int focusIndex() const;
    void moveFocus(int dx, int dy);

    int button(std::string key, PanelFrame frame, std::string text,
        std::function<void()> action, std::string tooltip = {}, bool enabled = true,
        bool selected = false, Treatment treatment = Treatment::Quiet, Color color = {236, 232, 214, 255},
        bool touchConfirmation = false, std::function<void()> preview = {});
    void panel(PanelFrame frame);
    void text(std::string value, int x, int y, int size = 18,
        Color color = {236, 232, 214, 255}, int width = 0, int lines = 1);
    void image(std::string group, int id, PanelFrame frame);
    void icon(Icon icon, PanelFrame frame, Color color = {221, 200, 141, 255});
    void drawIcon(Icon icon, PanelFrame frame, Color color);
    void synergyIcon(int iconId, PanelFrame frame, Color color);
    void disclosure(std::string key, PanelFrame frame, std::string label, std::function<void()> action);
    int iconButton(std::string key, PanelFrame frame, Icon icon, std::string label,
        std::function<void()> action, std::string tooltip = {}, bool enabled = true, bool selected = false,
        Color color = ChessUiStyle::ink, Treatment treatment = Treatment::Quiet);
    void pieceCard(const ChessSessionPiece& piece, PanelFrame frame, bool compact);
    void drawPieceCard(const PieceCard& card, PanelFrame frame);
    std::vector<std::pair<int, PanelFrame>> pieceEquipmentFrames(const PieceCard& card, PanelFrame frame) const;
    std::map<int, PanelFrame> cardFrames() const;
    void animateCardsFrom(const std::map<int, PanelFrame>& previous);
    void prose(const std::vector<PanelTextSourceRow>& rows, PanelFrame frame);
    void drawEquipmentPopover();
    void drawActionFeedback();
    void dismissEquipmentPopover();
};

} // namespace KysChess
