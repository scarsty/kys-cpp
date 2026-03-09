#pragma once

#include "ChessCombo.h"
#include "ChessDrawableOnCall.h"
#include "ChessEquipment.h"
#include "ChessManager.h"
#include "ChessNeigong.h"
#include "ChessProgress.h"
#include "ChessRoleSave.h"
#include "ChessRoster.h"
#include "ChessScreenLayout.h"
#include "DrawableOnCall.h"

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace KysChess
{

struct PanelTextCursor
{
    Font* font;
    int x;
    int y;

    void line(std::string text, int fontSize, Color color, int extraSpacing = 4, int indent = 0)
    {
        font->draw(text, fontSize, x + indent, y, color);
        y += fontSize + extraSpacing;
    }

    void skip(int spacing)
    {
        y += spacing;
    }
};

struct EquipmentDetailState
{
    int count = 0;
    std::vector<std::string> equippedBy;
};

using EquipmentDetailProvider = std::function<EquipmentDetailState(const EquipmentDef&)>;

class ComboInfoPanel : public ChessDrawableOnCall
{
public:
    explicit ComboInfoPanel(ChessManager manager);

private:
    void drawPanel();

    ChessManager manager_;
};

class OwnedRosterPanel : public DrawableOnCall
{
public:
    OwnedRosterPanel(ChessRoster& roster, ChessManager manager);

private:
    void drawPanel();

    ChessRoster& roster_;
    ChessManager manager_;
};

class ComboCatalogDetailPanel : public DrawableOnCall
{
public:
    ComboCatalogDetailPanel(const std::vector<ComboDef>& combos, ChessRoleSave& roleSave, std::map<int, int> starByRole);
    ComboCatalogDetailPanel(const std::vector<ComboDef>& combos, ChessRoleSave& roleSave, std::map<int, int> starByRole, PanelFrame frame);

private:
    void drawPanel();

    const std::vector<ComboDef>& combos_;
    ChessRoleSave& roleSave_;
    std::map<int, int> starByRole_;
    PanelFrame frame_;
};

class NeigongDetailPanel : public DrawableOnCall
{
public:
    explicit NeigongDetailPanel(std::vector<const NeigongDef*> neigongs);
    NeigongDetailPanel(std::vector<const NeigongDef*> neigongs, std::set<int> ownedMagicIds);
    NeigongDetailPanel(std::vector<const NeigongDef*> neigongs, PanelFrame frame);
    NeigongDetailPanel(std::vector<const NeigongDef*> neigongs, std::set<int> ownedMagicIds, PanelFrame frame);

private:
    void drawPanel();

    std::vector<const NeigongDef*> neigongs_;
    std::set<int> ownedMagicIds_;
    bool showOwnedState_ = false;
    PanelFrame frame_;
};

class EquipmentDetailPanel : public DrawableOnCall
{
public:
    EquipmentDetailPanel(std::vector<const EquipmentDef*> equipments, EquipmentDetailProvider detailProvider);
    EquipmentDetailPanel(std::vector<const EquipmentDef*> equipments, EquipmentDetailProvider detailProvider, PanelFrame frame);

private:
    void drawPanel();

    std::vector<const EquipmentDef*> equipments_;
    EquipmentDetailProvider detailProvider_;
    PanelFrame frame_;
};

class ChallengeDetailPanel : public DrawableOnCall
{
public:
    ChallengeDetailPanel(const std::vector<BalanceConfig::ChallengeDef>& challenges, ChessProgress& progress, ChessRoleSave& roleSave);
    ChallengeDetailPanel(const std::vector<BalanceConfig::ChallengeDef>& challenges, ChessProgress& progress, ChessRoleSave& roleSave, PanelFrame frame);

private:
    void drawPanel();

    const std::vector<BalanceConfig::ChallengeDef>& challenges_;
    ChessProgress& progress_;
    ChessRoleSave& roleSave_;
    PanelFrame frame_;
};

class BuyExpPreviewPanel : public DrawableOnCall
{
public:
    BuyExpPreviewPanel(
        std::string header,
        std::string costLine,
        std::string pieceLine,
        std::string pieceNext,
        std::string currentWeights,
        std::string nextWeights);

private:
    void drawPanel();

    std::string header_;
    std::string costLine_;
    std::string pieceLine_;
    std::string pieceNext_;
    std::string currentWeights_;
    std::string nextWeights_;
};

class PositionSwapInfoPanel : public DrawableOnCall
{
public:
    explicit PositionSwapInfoPanel(bool currentEnabled);

private:
    void drawPanel();

    bool currentEnabled_ = false;
};

class GuidePanel : public DrawableOnCall
{
public:
    GuidePanel();

private:
    void drawPanel();
};

}    // namespace KysChess